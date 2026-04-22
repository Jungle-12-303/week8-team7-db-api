#!/usr/bin/env python3
from __future__ import annotations

import argparse
import concurrent.futures
import json
import signal
import shlex
import socket
import subprocess
import sys
import threading
import time
from dataclasses import asdict, dataclass
from http.client import HTTPConnection, RemoteDisconnected, ResponseNotReady
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CASE_DIR = PROJECT_ROOT / "cases"
DEFAULT_QUERY_PATH = "/query"
DEFAULT_HEALTH_PATH = "/health"


@dataclass
class CaseResult:
    case_id: str
    title: str
    outcome: str
    passed: bool
    details: dict[str, Any]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run TEST-EDGE-FAILURE black-box scenarios against an API server."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--case-dir", default=str(DEFAULT_CASE_DIR))
    parser.add_argument("--case", dest="case_ids", action="append", help="Case id to run. Repeatable.")
    parser.add_argument("--list", action="store_true", help="List available cases and exit.")
    parser.add_argument("--query-path", default=DEFAULT_QUERY_PATH)
    parser.add_argument("--health-path", default=DEFAULT_HEALTH_PATH)
    parser.add_argument("--request-timeout-sec", type=float, default=2.5)
    parser.add_argument("--startup-timeout-sec", type=float, default=5.0)
    parser.add_argument("--restart-delay-sec", type=float, default=0.25)
    parser.add_argument("--max-sql-bytes", type=int, default=4096)
    parser.add_argument("--worker-count", type=int, default=4)
    parser.add_argument("--queue-capacity", type=int, default=8)
    parser.add_argument(
        "--server-command",
        help="Command used to launch a managed server process for setup, shutdown, and restart checks.",
    )
    parser.add_argument("--write-results", help="Write JSON results to the given path.")
    return parser.parse_args()


def load_cases(case_dir: Path) -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    for path in sorted(case_dir.glob("*.json")):
        with path.open("r", encoding="utf-8") as handle:
            payload = json.load(handle)
        payload["_path"] = str(path)
        cases.append(payload)
    return cases


def decode_bytes(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def split_command(command: str) -> list[str]:
    return shlex.split(command, posix=(sys.platform != "win32"))


def send_http_request(
    host: str,
    port: int,
    method: str,
    path: str,
    body: bytes = b"",
    headers: dict[str, str] | None = None,
    timeout_sec: float = 2.5,
) -> dict[str, Any]:
    connection = HTTPConnection(host, port, timeout=timeout_sec)
    request_headers = headers or {}
    try:
        connection.request(method, path, body=body, headers=request_headers)
        response = connection.getresponse()
        payload = response.read()
        return {
            "status": response.status,
            "reason": response.reason,
            "headers": dict(response.getheaders()),
            "body_text": decode_bytes(payload),
            "transport_error": None,
        }
    except (OSError, RemoteDisconnected, ResponseNotReady) as exc:
        return {
            "status": None,
            "reason": None,
            "headers": {},
            "body_text": "",
            "transport_error": str(exc),
        }
    finally:
        connection.close()


def health_probe(args: argparse.Namespace) -> dict[str, Any]:
    response = send_http_request(
        args.host,
        args.port,
        "GET",
        args.health_path,
        timeout_sec=min(1.5, args.request_timeout_sec),
    )
    response["ok"] = response["status"] == 200
    return response


def body_has_error_payload(body_text: str) -> bool:
    if not body_text.strip():
        return False
    try:
        parsed = json.loads(body_text)
    except json.JSONDecodeError:
        lowered = body_text.lower()
        return "error" in lowered or "message" in lowered or "detail" in lowered
    if isinstance(parsed, dict):
        return any(key in parsed for key in ("error", "message", "detail", "status"))
    return False


def parse_json_body(body_text: str) -> dict[str, Any] | None:
    if not body_text.strip():
        return None
    try:
        parsed = json.loads(body_text)
    except json.JSONDecodeError:
        return None
    return parsed if isinstance(parsed, dict) else None


def count_statuses(results: list[dict[str, Any]]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for result in results:
        if result["status"] is None:
            label = "transport_error"
        else:
            label = str(result["status"])
        counts[label] = counts.get(label, 0) + 1
    return counts


def repeated_sql(seed_sql: str, target_bytes: int) -> bytes:
    seed = seed_sql.encode("utf-8")
    if not seed:
        seed = b"SELECT 1;"
    chunks: list[bytes] = []
    size = 0
    while size < target_bytes:
        chunks.append(seed)
        size += len(seed)
    return b"".join(chunks)


class ManagedServer:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.process: subprocess.Popen[bytes] | None = None

    def start(self) -> None:
        if not self.args.server_command:
            return
        if self.process and self.process.poll() is None:
            return
        command = split_command(self.args.server_command)
        creationflags = 0
        if sys.platform == "win32":
            creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
        self.process = subprocess.Popen(
            command,
            cwd=str(PROJECT_ROOT),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=creationflags,
        )
        deadline = time.monotonic() + self.args.startup_timeout_sec
        last_probe: dict[str, Any] | None = None
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError("Managed server exited before becoming healthy.")
            probe = health_probe(self.args)
            if probe["ok"]:
                return
            last_probe = probe
            time.sleep(0.1)
        raise RuntimeError(f"Managed server did not become healthy: {last_probe}")

    def request_stop(self, graceful: bool = True) -> None:
        if not self.process:
            return
        if self.process.poll() is not None:
            self.process = None
            return
        sent_signal = False
        if graceful:
            try:
                if sys.platform == "win32":
                    ctrl_break = getattr(signal, "CTRL_BREAK_EVENT", None)
                    if ctrl_break is not None:
                        self.process.send_signal(ctrl_break)
                        sent_signal = True
                else:
                    self.process.send_signal(signal.SIGTERM)
                    sent_signal = True
            except (OSError, ValueError):
                sent_signal = False
        if not sent_signal:
            self.process.terminate()
        try:
            self.process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=2.0)
        self.process = None

    def stop(self) -> None:
        self.request_stop(graceful=False)

    def restart(self) -> None:
        self.stop()
        time.sleep(self.args.restart_delay_sec)
        self.start()


def finalize_case(
    case: dict[str, Any],
    args: argparse.Namespace,
    passed: bool,
    details: dict[str, Any],
) -> CaseResult:
    expect = case.get("expect", {})
    if expect.get("server_alive_after"):
        probe = health_probe(args)
        details["health_after"] = {
            "status": probe["status"],
            "transport_error": probe["transport_error"],
            "ok": probe["ok"],
        }
        passed = passed and probe["ok"]
    return CaseResult(
        case_id=case["id"],
        title=case["title"],
        outcome="PASS" if passed else "FAIL",
        passed=passed,
        details=details,
    )


def skipped_case(case: dict[str, Any], reason: str) -> CaseResult:
    return CaseResult(
        case_id=case["id"],
        title=case["title"],
        outcome="SKIP",
        passed=False,
        details={"reason": reason},
    )


def run_empty_body_case(case: dict[str, Any], args: argparse.Namespace) -> CaseResult:
    expect = case["expect"]
    response = send_http_request(
        args.host,
        args.port,
        "POST",
        args.query_path,
        body=b"",
        headers={"Content-Type": "text/plain; charset=utf-8"},
        timeout_sec=args.request_timeout_sec,
    )
    passed = response["status"] in expect["statuses"]
    if expect.get("require_error_payload"):
        passed = passed and body_has_error_payload(response["body_text"])
    details = {
        "status": response["status"],
        "transport_error": response["transport_error"],
        "body_preview": response["body_text"][:200],
    }
    return finalize_case(case, args, passed, details)


def run_oversized_sql_case(case: dict[str, Any], args: argparse.Namespace) -> CaseResult:
    expect = case["expect"]
    target_bytes = max(case.get("minimum_bytes", 0), args.max_sql_bytes * case.get("multiplier", 2))
    body = repeated_sql(case.get("seed_sql", "SELECT * FROM student;"), target_bytes)
    response = send_http_request(
        args.host,
        args.port,
        "POST",
        args.query_path,
        body=body,
        headers={"Content-Type": "text/plain; charset=utf-8"},
        timeout_sec=max(args.request_timeout_sec, 5.0),
    )
    passed = response["status"] in expect["statuses"]
    if expect.get("require_error_payload"):
        passed = passed and body_has_error_payload(response["body_text"])
    details = {
        "status": response["status"],
        "transport_error": response["transport_error"],
        "sent_bytes": len(body),
        "body_preview": response["body_text"][:200],
    }
    return finalize_case(case, args, passed, details)


def run_client_disconnect_case(case: dict[str, Any], args: argparse.Namespace) -> CaseResult:
    sql = case.get("sql", "SELECT * FROM student;").encode("utf-8")
    partial_bytes = min(case.get("partial_bytes", 8), len(sql))
    declared_length = max(len(sql), partial_bytes * 4)
    details: dict[str, Any] = {
        "sent_bytes": partial_bytes,
        "declared_length": declared_length,
    }
    passed = True
    try:
        with socket.create_connection((args.host, args.port), timeout=args.request_timeout_sec) as sock:
            request = (
                f"POST {args.query_path} HTTP/1.1\r\n"
                f"Host: {args.host}:{args.port}\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                f"Content-Length: {declared_length}\r\n"
                "Connection: close\r\n"
                "\r\n"
            ).encode("ascii")
            sock.sendall(request)
            sock.sendall(sql[:partial_bytes])
    except OSError as exc:
        passed = False
        details["transport_error"] = str(exc)
    time.sleep(case.get("settle_sec", 0.3))
    return finalize_case(case, args, passed, details)


def run_burst_load_case(case: dict[str, Any], args: argparse.Namespace) -> CaseResult:
    expect = case["expect"]
    concurrency = max(
        1,
        args.worker_count * case.get("concurrency_multiplier", 1)
        + args.queue_capacity
        + case.get("burst_extra", 0),
    )
    timeout_sec = case.get("request_timeout_sec", args.request_timeout_sec)
    request_headers = {"Content-Type": "text/plain; charset=utf-8"}
    if case.get("mock_sleep_ms"):
        request_headers["X-Mock-Sleep-Ms"] = str(case["mock_sleep_ms"])
    body = case.get("sql", "SELECT * FROM student;").encode("utf-8")

    def do_request(_: int) -> dict[str, Any]:
        return send_http_request(
            args.host,
            args.port,
            "POST",
            args.query_path,
            body=body,
            headers=request_headers,
            timeout_sec=timeout_sec,
        )

    started = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as executor:
        futures = [executor.submit(do_request, index) for index in range(concurrency)]
        results = [future.result() for future in futures]
    elapsed = round(time.monotonic() - started, 3)

    allowed_statuses = set(expect.get("allowed_statuses", []))
    overload_statuses = set(expect.get("overload_statuses", []))
    require_overload = expect.get("require_overload", False)

    status_counts = count_statuses(results)
    all_allowed = all(
        (result["status"] in allowed_statuses) if result["status"] is not None else False
        for result in results
    )
    overload_seen = any(result["status"] in overload_statuses for result in results)
    passed = all_allowed and (overload_seen if require_overload else True)
    details = {
        "request_count": concurrency,
        "elapsed_sec": elapsed,
        "status_counts": status_counts,
    }
    return finalize_case(case, args, passed, details)


def run_invalid_sql_case(case: dict[str, Any], args: argparse.Namespace) -> CaseResult:
    expect = case["expect"]
    response = send_http_request(
        args.host,
        args.port,
        "POST",
        args.query_path,
        body=case.get("sql", "SELECT FROM ;").encode("utf-8"),
        headers={"Content-Type": "text/plain; charset=utf-8"},
        timeout_sec=args.request_timeout_sec,
    )
    passed = response["status"] in expect["statuses"]
    if expect.get("require_error_payload"):
        passed = passed and body_has_error_payload(response["body_text"])
    details = {
        "status": response["status"],
        "transport_error": response["transport_error"],
        "body_preview": response["body_text"][:200],
    }
    return finalize_case(case, args, passed, details)


def run_invalid_debug_header_case(case: dict[str, Any], args: argparse.Namespace) -> CaseResult:
    expect = case["expect"]
    response = send_http_request(
        args.host,
        args.port,
        "POST",
        args.query_path,
        body=case.get("sql", "SELECT * FROM student;").encode("utf-8"),
        headers={
            "Content-Type": "text/plain; charset=utf-8",
            case.get("header_name", "X-Debug-Sleep-Ms"): case.get("header_value", "10001"),
        },
        timeout_sec=args.request_timeout_sec,
    )
    parsed_body = parse_json_body(response["body_text"])
    passed = response["status"] in expect["statuses"]
    if expect.get("require_error_payload"):
        passed = passed and body_has_error_payload(response["body_text"])
    required_error_code = expect.get("require_error_code")
    if required_error_code:
        passed = passed and parsed_body is not None and parsed_body.get("error_code") == required_error_code
    details = {
        "status": response["status"],
        "transport_error": response["transport_error"],
        "body_preview": response["body_text"][:200],
    }
    if parsed_body is not None:
        details["error_code"] = parsed_body.get("error_code")
    return finalize_case(case, args, passed, details)


def send_streaming_request(
    host: str,
    port: int,
    path: str,
    body: bytes,
    chunk_size: int,
    chunk_delay_ms: int,
    timeout_sec: float,
    result: dict[str, Any],
) -> None:
    try:
        with socket.create_connection((host, port), timeout=timeout_sec) as sock:
            sock.settimeout(timeout_sec)
            request = (
                f"POST {path} HTTP/1.1\r\n"
                f"Host: {host}:{port}\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                f"Content-Length: {len(body)}\r\n"
                "Connection: close\r\n"
                "\r\n"
            ).encode("ascii")
            sock.sendall(request)
            offset = 0
            while offset < len(body):
                next_offset = min(offset + chunk_size, len(body))
                sock.sendall(body[offset:next_offset])
                offset = next_offset
                time.sleep(chunk_delay_ms / 1000.0)
            chunks: list[bytes] = []
            while True:
                data = sock.recv(4096)
                if not data:
                    break
                chunks.append(data)
    except OSError as exc:
        result["transport_error"] = str(exc)
        return

    raw_response = decode_bytes(b"".join(chunks))
    first_line = raw_response.splitlines()[0] if raw_response else ""
    status = None
    if first_line.startswith("HTTP/"):
        parts = first_line.split()
        if len(parts) >= 2 and parts[1].isdigit():
            status = int(parts[1])
    result["status"] = status
    result["body_preview"] = raw_response[:200]


def run_shutdown_during_request_case(
    case: dict[str, Any],
    args: argparse.Namespace,
    manager: ManagedServer,
) -> CaseResult:
    if not args.server_command:
        return skipped_case(case, "shutdown-during-request requires --server-command")

    expect = case["expect"]
    target_bytes = max(case.get("stream_body_bytes", 32768), args.max_sql_bytes * 2)
    body = repeated_sql(case.get("seed_sql", "SELECT * FROM student;"), target_bytes)
    request_result: dict[str, Any] = {}
    worker = threading.Thread(
        target=send_streaming_request,
        args=(
            args.host,
            args.port,
            args.query_path,
            body,
            case.get("chunk_size", 512),
            case.get("chunk_delay_ms", 75),
            max(args.request_timeout_sec, 4.0),
            request_result,
        ),
        daemon=True,
    )
    worker.start()
    time.sleep(case.get("shutdown_after_sec", 0.35))
    manager.request_stop(graceful=True)
    worker.join(timeout=max(args.request_timeout_sec, 4.0))
    if case.get("restart_after", True):
        manager.start()

    status = request_result.get("status")
    transport_error = request_result.get("transport_error")
    status_allowed = status in set(expect.get("allowed_statuses", []))
    disconnect_allowed = expect.get("accept_disconnect", False) and bool(transport_error)
    passed = status_allowed or disconnect_allowed
    details = {
        "status": status,
        "transport_error": transport_error,
        "body_preview": request_result.get("body_preview", ""),
        "restarted": case.get("restart_after", True),
    }
    return finalize_case(case, args, passed, details)


def run_case(case: dict[str, Any], args: argparse.Namespace, manager: ManagedServer) -> CaseResult:
    case_type = case["type"]
    if case_type == "empty_body":
        return run_empty_body_case(case, args)
    if case_type == "oversized_sql":
        return run_oversized_sql_case(case, args)
    if case_type == "client_disconnect":
        return run_client_disconnect_case(case, args)
    if case_type == "burst_load":
        return run_burst_load_case(case, args)
    if case_type == "shutdown_during_request":
        return run_shutdown_during_request_case(case, args, manager)
    if case_type == "invalid_sql":
        return run_invalid_sql_case(case, args)
    if case_type == "invalid_debug_header":
        return run_invalid_debug_header_case(case, args)
    return skipped_case(case, f"Unsupported case type: {case_type}")


def select_cases(cases: list[dict[str, Any]], case_ids: list[str] | None) -> list[dict[str, Any]]:
    if not case_ids:
        return cases
    wanted = set(case_ids)
    selected = [case for case in cases if case["id"] in wanted]
    missing = wanted - {case["id"] for case in selected}
    if missing:
        raise ValueError(f"Unknown case ids: {', '.join(sorted(missing))}")
    return selected


def print_case_list(cases: list[dict[str, Any]]) -> None:
    for case in cases:
        print(f"{case['id']:24} {case['type']:24} {case['title']}")


def print_result(result: CaseResult) -> None:
    details = result.details
    summary = []
    if "status" in details and details["status"] is not None:
        summary.append(f"status={details['status']}")
    if "status_counts" in details:
        summary.append(f"counts={details['status_counts']}")
    if "transport_error" in details and details["transport_error"]:
        summary.append(f"transport_error={details['transport_error']}")
    if "health_after" in details:
        summary.append(f"health_after={details['health_after']}")
    joined = ", ".join(summary)
    print(f"{result.outcome:4} {result.case_id:24} {joined}".rstrip())


def main() -> int:
    args = parse_args()
    case_dir = Path(args.case_dir)
    cases = load_cases(case_dir)
    if args.list:
        print_case_list(cases)
        return 0

    try:
        selected_cases = select_cases(cases, args.case_ids)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    manager = ManagedServer(args)
    try:
        if args.server_command:
            manager.start()
        else:
            probe = health_probe(args)
            if not probe["ok"]:
                print(f"Server is not healthy before test run: {probe}", file=sys.stderr)
                return 2

        results: list[CaseResult] = []
        for case in selected_cases:
            result = run_case(case, args, manager)
            results.append(result)
            print_result(result)
    finally:
        manager.stop()

    passed = sum(1 for result in results if result.outcome == "PASS")
    failed = sum(1 for result in results if result.outcome == "FAIL")
    skipped = sum(1 for result in results if result.outcome == "SKIP")
    print(f"Summary: pass={passed}, fail={failed}, skip={skipped}")

    if args.write_results:
        output_path = Path(args.write_results)
        output_path.write_text(
            json.dumps([asdict(result) for result in results], ensure_ascii=False, indent=2),
            encoding="utf-8",
        )

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
