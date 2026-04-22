#!/usr/bin/env python3
from __future__ import annotations

import argparse
import concurrent.futures
import copy
import json
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib import error as urllib_error
from urllib import request as urllib_request


TEMPLATE_PATTERN = re.compile(r"\{\{([a-zA-Z0-9_]+)\}\}")


class ReadWriteGate:
    def __init__(self) -> None:
        self._condition = threading.Condition()
        self._active_readers = 0
        self._writer_active = False

    def acquire_read(self) -> None:
        with self._condition:
            while self._writer_active:
                self._condition.wait()
            self._active_readers += 1

    def release_read(self) -> None:
        with self._condition:
            self._active_readers -= 1
            if self._active_readers == 0:
                self._condition.notify_all()

    def acquire_write(self) -> None:
        with self._condition:
            while self._writer_active or self._active_readers > 0:
                self._condition.wait()
            self._writer_active = True

    def release_write(self) -> None:
        with self._condition:
            self._writer_active = False
            self._condition.notify_all()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run one or more TEST-CONCURRENCY case files against the DB HTTP server."
    )
    parser.add_argument("case_paths", nargs="*", help="One or more case JSON files.")
    parser.add_argument("--base-url", help="Override base URL such as http://127.0.0.1:8080")
    parser.add_argument("--timeout", type=float, default=10.0, help="Per-request timeout in seconds.")
    parser.add_argument("--skip-health-check", action="store_true", help="Skip GET /health before each case.")
    parser.add_argument("--output-json", help="Write the aggregated report to a JSON file.")
    parser.add_argument(
        "--print-response-bodies",
        action="store_true",
        help="Print every response body in the console report.",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run the built-in mock server and execute all shipped cases against it.",
    )
    return parser.parse_args()


def render_template(value: Any, variables: dict[str, str]) -> Any:
    if isinstance(value, str):
        return TEMPLATE_PATTERN.sub(lambda match: variables.get(match.group(1), match.group(0)), value)
    if isinstance(value, list):
        return [render_template(item, variables) for item in value]
    if isinstance(value, dict):
        return {key: render_template(item, variables) for key, item in value.items()}
    return value


def load_case(case_path: Path) -> dict[str, Any]:
    with case_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def ensure_trailing_slash(value: str) -> str:
    return value if value.endswith("/") else f"{value}/"


def join_url(base_url: str, path: str) -> str:
    return ensure_trailing_slash(base_url.rstrip("/")) + path.lstrip("/")


def decode_response_body(raw_body: bytes, headers: dict[str, str]) -> str:
    content_type = headers.get("Content-Type", "")
    match = re.search(r"charset=([a-zA-Z0-9_-]+)", content_type)
    encoding = match.group(1) if match else "utf-8"
    try:
        return raw_body.decode(encoding, errors="replace")
    except LookupError:
        return raw_body.decode("utf-8", errors="replace")


def try_parse_json(body_text: str) -> Any:
    if not body_text:
        return None
    try:
        return json.loads(body_text)
    except json.JSONDecodeError:
        return None


def json_path_get(value: Any, path: str) -> tuple[bool, Any]:
    current = value
    if path == "":
        return True, current

    for token in path.split("."):
        if isinstance(current, list):
            try:
                index = int(token)
            except ValueError:
                return False, None
            if index < 0 or index >= len(current):
                return False, None
            current = current[index]
            continue

        if isinstance(current, dict):
            if token not in current:
                return False, None
            current = current[token]
            continue

        return False, None

    return True, current


def build_default_body(target: dict[str, Any], request_spec: dict[str, Any], variables: dict[str, str]) -> Any:
    if "body" in request_spec:
        return render_template(copy.deepcopy(request_spec["body"]), variables)

    if "body_template" in target:
        target_variables = dict(variables)
        if "sql" in request_spec:
            target_variables["sql"] = request_spec["sql"]
        return render_template(copy.deepcopy(target["body_template"]), target_variables)

    if "sql" in request_spec:
        return {"sql": request_spec["sql"]}

    return None


def perform_request(
    *,
    base_url: str,
    target: dict[str, Any],
    request_spec: dict[str, Any],
    variables: dict[str, str],
    timeout: float,
) -> dict[str, Any]:
    method = str(request_spec.get("method", "POST")).upper()
    path = str(request_spec.get("path", target.get("query_path", "/query")))
    url = join_url(base_url, path)
    headers = dict(target.get("headers", {}))
    headers.update(request_spec.get("headers", {}))
    body_value = build_default_body(target, request_spec, variables)
    body_bytes = None

    if body_value is not None:
        if isinstance(body_value, (dict, list)):
            body_bytes = json.dumps(body_value, ensure_ascii=False).encode("utf-8")
            headers.setdefault("Content-Type", "application/json")
        elif isinstance(body_value, str):
            body_bytes = body_value.encode("utf-8")
        else:
            body_bytes = str(body_value).encode("utf-8")

    request = urllib_request.Request(url, data=body_bytes, headers=headers, method=method)
    started_at = time.perf_counter()
    status = None
    response_headers: dict[str, str] = {}
    body_text = ""
    transport_error = None

    try:
        with urllib_request.urlopen(request, timeout=timeout) as response:
            status = response.getcode()
            response_headers = dict(response.headers.items())
            body_text = decode_response_body(response.read(), response_headers)
    except urllib_error.HTTPError as exc:
        status = exc.code
        response_headers = dict(exc.headers.items())
        body_text = decode_response_body(exc.read(), response_headers)
    except urllib_error.URLError as exc:
        transport_error = f"URLError: {exc.reason}"
    except Exception as exc:  # pragma: no cover - defensive fallback
        transport_error = f"{type(exc).__name__}: {exc}"

    ended_at = time.perf_counter()
    json_body = try_parse_json(body_text)
    expectation = request_spec.get("expect", {})
    success, failures = evaluate_expectation(
        expectation=expectation,
        status=status,
        body_text=body_text,
        json_body=json_body,
        transport_error=transport_error,
    )

    return {
        "name": request_spec.get("name", variables["request_name"]),
        "sql": request_spec.get("sql"),
        "method": method,
        "url": url,
        "status": status,
        "body": body_text,
        "json": json_body,
        "headers": response_headers,
        "transport_error": transport_error,
        "started_at": started_at,
        "ended_at": ended_at,
        "elapsed_ms": round((ended_at - started_at) * 1000.0, 3),
        "success": success,
        "expectation_failures": failures,
    }


def evaluate_expectation(
    *,
    expectation: dict[str, Any],
    status: int | None,
    body_text: str,
    json_body: Any,
    transport_error: str | None,
) -> tuple[bool, list[str]]:
    failures: list[str] = []

    if transport_error is not None:
        failures.append(transport_error)
        return False, failures

    if expectation:
        if "status" in expectation and status != expectation["status"]:
            failures.append(f"expected status {expectation['status']}, got {status}")
        if "statuses" in expectation and status not in expectation["statuses"]:
            failures.append(f"expected one of {expectation['statuses']}, got {status}")
    elif status is None or status < 200 or status >= 300:
        failures.append(f"expected default 2xx status, got {status}")

    for check in expectation.get("json_equals", []):
        exists, actual = json_path_get(json_body, check["path"])
        if not exists:
            failures.append(f"missing JSON path {check['path']}")
            continue
        if actual != check["equals"]:
            failures.append(f"expected JSON {check['path']} == {check['equals']!r}, got {actual!r}")

    for check in expectation.get("json_string_contains", []):
        exists, actual = json_path_get(json_body, check["path"])
        if not exists:
            failures.append(f"missing JSON path {check['path']}")
            continue
        actual_text = "" if actual is None else str(actual)
        if check["substring"] not in actual_text:
            failures.append(
                f"expected substring {check['substring']!r} in JSON {check['path']}, got {actual_text!r}"
            )

    for check in expectation.get("json_string_occurrence_counts", []):
        exists, actual = json_path_get(json_body, check["path"])
        if not exists:
            failures.append(f"missing JSON path {check['path']}")
            continue
        actual_text = "" if actual is None else str(actual)
        count = actual_text.count(check["substring"])
        if count != check["equals"]:
            failures.append(
                f"expected substring {check['substring']!r} in JSON {check['path']} to appear "
                f"{check['equals']} time(s), got {count}"
            )

    for substring in expectation.get("body_contains", []):
        if substring not in body_text:
            failures.append(f"expected substring {substring!r} in body")

    for substring in expectation.get("body_not_contains", []):
        if substring in body_text:
            failures.append(f"unexpected substring {substring!r} in body")

    for check in expectation.get("body_occurrence_counts", []):
        count = body_text.count(check["substring"])
        if count != check["equals"]:
            failures.append(
                f"expected body substring {check['substring']!r} to appear {check['equals']} time(s), got {count}"
            )

    return len(failures) == 0, failures


def compute_phase_parallelism(requests: list[dict[str, Any]]) -> int:
    events: list[tuple[float, int]] = []
    for request in requests:
        events.append((request["started_at"], 1))
        events.append((request["ended_at"], -1))

    events.sort(key=lambda item: (item[0], item[1]))
    active = 0
    max_parallel = 0

    for _, delta in events:
        active += delta
        if active > max_parallel:
            max_parallel = active

    return max_parallel


def build_request_variables(run_id: str, phase_name: str, request_index: int, request_name: str) -> dict[str, str]:
    return {
        "run_id": run_id,
        "phase_name": phase_name,
        "request_index": str(request_index),
        "request_name": request_name,
    }


def run_phase(
    *,
    phase: dict[str, Any],
    target: dict[str, Any],
    base_url: str,
    run_id: str,
    timeout: float,
) -> dict[str, Any]:
    requests = phase.get("requests", [])
    rendered_requests: list[dict[str, Any]] = []

    for request_index, raw_request in enumerate(requests):
        request_name = raw_request.get("name", f"{phase['name']}-{request_index}")
        variables = build_request_variables(run_id, phase["name"], request_index, request_name)
        rendered_request = render_template(copy.deepcopy(raw_request), variables)
        rendered_request.setdefault("name", request_name)
        rendered_requests.append(rendered_request)

    started_at = time.perf_counter()
    results: list[dict[str, Any]] = []

    if phase.get("mode") == "concurrent":
        start_gate = threading.Event()
        max_workers = int(phase.get("max_workers", max(len(rendered_requests), 1)))

        def run_one(request_spec: dict[str, Any], request_index: int) -> dict[str, Any]:
            request_name = request_spec.get("name", f"{phase['name']}-{request_index}")
            variables = build_request_variables(run_id, phase["name"], request_index, request_name)
            start_gate.wait()
            return perform_request(
                base_url=base_url,
                target=target,
                request_spec=request_spec,
                variables=variables,
                timeout=timeout,
            )

        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = [
                executor.submit(run_one, request_spec, request_index)
                for request_index, request_spec in enumerate(rendered_requests)
            ]
            start_gate.set()
            for future in concurrent.futures.as_completed(futures):
                results.append(future.result())
    else:
        for request_index, request_spec in enumerate(rendered_requests):
            request_name = request_spec.get("name", f"{phase['name']}-{request_index}")
            variables = build_request_variables(run_id, phase["name"], request_index, request_name)
            results.append(
                perform_request(
                    base_url=base_url,
                    target=target,
                    request_spec=request_spec,
                    variables=variables,
                    timeout=timeout,
                )
            )

    ended_at = time.perf_counter()
    ordered_results = sorted(results, key=lambda item: item["started_at"])
    success_count = sum(1 for item in ordered_results if item["success"])
    max_parallel = compute_phase_parallelism(ordered_results) if ordered_results else 0

    return {
        "name": phase["name"],
        "mode": phase.get("mode", "sequential"),
        "request_count": len(ordered_results),
        "success_count": success_count,
        "failure_count": len(ordered_results) - success_count,
        "elapsed_ms": round((ended_at - started_at) * 1000.0, 3),
        "max_parallel": max_parallel,
        "requests": ordered_results,
    }


def resolve_base_url(target: dict[str, Any], override_base_url: str | None) -> str:
    if override_base_url:
        return override_base_url.rstrip("/")

    base_url_env = target.get("base_url_env")
    if base_url_env and os.environ.get(base_url_env):
        return str(os.environ[base_url_env]).rstrip("/")

    base_url = target.get("base_url")
    if base_url:
        return str(base_url).rstrip("/")

    raise ValueError("base URL is required via --base-url, target.base_url, or target.base_url_env")


def run_health_check(base_url: str, target: dict[str, Any], timeout: float) -> dict[str, Any]:
    url = join_url(base_url, str(target.get("health_path", "/health")))
    request = urllib_request.Request(url, method="GET")
    started_at = time.perf_counter()
    status = None
    transport_error = None
    body_text = ""

    try:
        with urllib_request.urlopen(request, timeout=timeout) as response:
            status = response.getcode()
            body_text = decode_response_body(response.read(), dict(response.headers.items()))
    except urllib_error.HTTPError as exc:
        status = exc.code
        body_text = decode_response_body(exc.read(), dict(exc.headers.items()))
    except urllib_error.URLError as exc:
        transport_error = f"URLError: {exc.reason}"
    except Exception as exc:  # pragma: no cover - defensive fallback
        transport_error = f"{type(exc).__name__}: {exc}"

    ended_at = time.perf_counter()
    ok = transport_error is None and status is not None and 200 <= status < 300

    return {
        "url": url,
        "status": status,
        "body": body_text,
        "transport_error": transport_error,
        "elapsed_ms": round((ended_at - started_at) * 1000.0, 3),
        "ok": ok,
    }


def run_assertions(case_data: dict[str, Any], phase_results: list[dict[str, Any]]) -> list[dict[str, Any]]:
    phase_map = {phase["name"]: phase for phase in phase_results}
    assertion_results: list[dict[str, Any]] = []

    for index, assertion in enumerate(case_data.get("assertions", [])):
        assertion_type = assertion["type"]
        success = False
        message = ""

        try:
            if assertion_type == "phase_requests_success":
                phase = phase_map[assertion["phase"]]
                success = phase["failure_count"] == 0
                message = (
                    f"{assertion['phase']} expected all requests to succeed, "
                    f"got {phase['success_count']}/{phase['request_count']}"
                )
            elif assertion_type == "concurrent_shorter_or_overlap":
                phase = phase_map[assertion["phase"]]
                baseline = phase_map[assertion["baseline"]]
                if baseline["elapsed_ms"] <= 0:
                    success = phase["max_parallel"] > 1
                else:
                    ratio = phase["elapsed_ms"] / baseline["elapsed_ms"]
                    success = ratio <= float(assertion["max_ratio"]) or phase["max_parallel"] > 1
                    message = (
                        f"{assertion['phase']} ratio={ratio:.3f}, "
                        f"max_parallel={phase['max_parallel']}, baseline={assertion['baseline']}"
                    )
            elif assertion_type == "elapsed_ratio_at_least":
                phase = phase_map[assertion["phase"]]
                baseline = phase_map[assertion["baseline"]]
                ratio = phase["elapsed_ms"] / baseline["elapsed_ms"] if baseline["elapsed_ms"] > 0 else 0.0
                success = ratio >= float(assertion["min_ratio"])
                message = (
                    f"{assertion['phase']} ratio={ratio:.3f}, "
                    f"required>={float(assertion['min_ratio']):.3f}, baseline={assertion['baseline']}"
                )
            elif assertion_type == "elapsed_ratio_at_most":
                phase = phase_map[assertion["phase"]]
                baseline = phase_map[assertion["baseline"]]
                ratio = phase["elapsed_ms"] / baseline["elapsed_ms"] if baseline["elapsed_ms"] > 0 else 0.0
                success = ratio <= float(assertion["max_ratio"])
                message = (
                    f"{assertion['phase']} ratio={ratio:.3f}, "
                    f"required<={float(assertion['max_ratio']):.3f}, baseline={assertion['baseline']}"
                )
            elif assertion_type == "phase_has_overlap":
                phase = phase_map[assertion["phase"]]
                success = phase["max_parallel"] > 1
                message = f"{assertion['phase']} max_parallel={phase['max_parallel']}"
            else:
                message = f"unsupported assertion type {assertion_type}"
        except KeyError as exc:
            message = f"missing phase or field for assertion {assertion_type}: {exc}"

        if not message:
            message = f"{assertion_type} passed"

        assertion_results.append(
            {
                "name": assertion.get("name", f"assertion-{index}"),
                "type": assertion_type,
                "success": success,
                "message": message,
            }
        )

    return assertion_results


def run_case_file(
    *,
    case_path: Path,
    base_url_override: str | None,
    timeout: float,
    skip_health_check: bool,
) -> dict[str, Any]:
    case_data = load_case(case_path)
    target = case_data.get("target", {})
    base_url = resolve_base_url(target, base_url_override)
    run_id = time.strftime("%Y%m%d%H%M%S") + f"{time.time_ns() % 1_000_000:06d}"

    health_check = None
    if not skip_health_check:
        health_check = run_health_check(base_url, target, timeout)
        if not health_check["ok"]:
            return {
                "case_path": str(case_path),
                "case_name": case_data.get("name", case_path.stem),
                "description": case_data.get("description", ""),
                "base_url": base_url,
                "run_id": run_id,
                "health_check": health_check,
                "phases": [],
                "assertions": [],
                "ok": False,
            }

    phase_results = [
        run_phase(phase=phase, target=target, base_url=base_url, run_id=run_id, timeout=timeout)
        for phase in case_data.get("phases", [])
    ]
    assertion_results = run_assertions(case_data, phase_results)
    ok = all(assertion["success"] for assertion in assertion_results)

    return {
        "case_path": str(case_path),
        "case_name": case_data.get("name", case_path.stem),
        "description": case_data.get("description", ""),
        "base_url": base_url,
        "run_id": run_id,
        "health_check": health_check,
        "phases": phase_results,
        "assertions": assertion_results,
        "ok": ok,
    }


def print_report(report: dict[str, Any], print_response_bodies: bool) -> None:
    outcome = "PASS" if report["ok"] else "FAIL"
    print(f"[CASE] {report['case_name']} {outcome}")
    print(f"  path: {report['case_path']}")
    print(f"  base_url: {report['base_url']}")
    print(f"  run_id: {report['run_id']}")

    health_check = report.get("health_check")
    if health_check is not None:
        print(
            f"  health: status={health_check['status']} ok={health_check['ok']} "
            f"elapsed_ms={health_check['elapsed_ms']}"
        )
        if health_check["transport_error"]:
            print(f"    transport_error: {health_check['transport_error']}")

    for phase in report["phases"]:
        print(
            f"  phase {phase['name']} [{phase['mode']}]: "
            f"{phase['success_count']}/{phase['request_count']} passed, "
            f"elapsed_ms={phase['elapsed_ms']}, max_parallel={phase['max_parallel']}"
        )
        for request in phase["requests"]:
            request_outcome = "ok" if request["success"] else "fail"
            print(
                f"    {request['name']}: {request_outcome}, status={request['status']}, "
                f"elapsed_ms={request['elapsed_ms']}"
            )
            if request["transport_error"]:
                print(f"      transport_error: {request['transport_error']}")
            for failure in request["expectation_failures"]:
                print(f"      expectation_failure: {failure}")
            if print_response_bodies:
                print(f"      body: {request['body']}")

    for assertion in report["assertions"]:
        assertion_outcome = "ok" if assertion["success"] else "fail"
        print(f"  assertion {assertion['type']}: {assertion_outcome} ({assertion['message']})")


class MockServerState:
    def __init__(self) -> None:
        self.gate = ReadWriteGate()
        self.inserted_names: set[str] = set()


class MockRequestHandler(BaseHTTPRequestHandler):
    server_version = "TestConcurrencyMock/1.0"
    protocol_version = "HTTP/1.1"

    def log_message(self, format: str, *args: Any) -> None:  # noqa: A003
        return

    @property
    def state(self) -> MockServerState:
        return self.server.state  # type: ignore[attr-defined]

    def _send_json(self, status_code: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        if self.path != "/health":
            self._send_json(
                404,
                {
                    "ok": False,
                    "path": self.path,
                    "status_code": 404,
                    "error_code": "path_not_found",
                    "message": "unsupported endpoint",
                },
            )
            return
        self._send_json(
            200,
            {
                "ok": True,
                "path": "/health",
                "status_code": 200,
                "message": "server is healthy",
            },
        )

    def do_POST(self) -> None:  # noqa: N802
        if self.path != "/query":
            self._send_json(
                404,
                {
                    "ok": False,
                    "path": self.path,
                    "status_code": 404,
                    "error_code": "path_not_found",
                    "message": "unsupported endpoint",
                },
            )
            return

        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length)
        content_type = self.headers.get("Content-Type", "")

        if "application/json" in content_type.lower():
            try:
                body = json.loads(raw_body.decode("utf-8"))
            except json.JSONDecodeError:
                self._send_json(
                    400,
                    {
                        "ok": False,
                        "path": "/query",
                        "status_code": 400,
                        "error_code": "invalid_body",
                        "message": "request body must contain a SQL statement",
                    },
                )
                return

            sql = body.get("sql")
            if not isinstance(sql, str) or not sql.strip():
                self._send_json(
                    400,
                    {
                        "ok": False,
                        "path": "/query",
                        "status_code": 400,
                        "error_code": "invalid_body",
                        "message": "request body must contain a SQL statement",
                    },
                )
                return
        else:
            sql = raw_body.decode("utf-8", errors="replace")
            if not sql.strip():
                self._send_json(
                    400,
                    {
                        "ok": False,
                        "path": "/query",
                        "status_code": 400,
                        "error_code": "invalid_body",
                        "message": "request body must contain a SQL statement",
                    },
                )
                return

        normalized = sql.strip().upper()
        if normalized.startswith("SELECT"):
            self._handle_select(sql)
            return
        if normalized.startswith("INSERT"):
            self._handle_insert(sql)
            return

        self._send_json(
            400,
            {
                "ok": False,
                "path": "/query",
                "status_code": 400,
                "error_code": "query_failed",
                "message": "unsupported_sql",
                "affected_rows": 0,
                "output_text": "",
            },
        )

    def _handle_select(self, sql: str) -> None:
        self.state.gate.acquire_read()
        try:
            upper_sql = sql.upper()
            output = "SELECT 0"
            time.sleep(0.05)

            name_match = re.search(r"WHERE\s+name\s*=\s*'([^']+)'", sql, flags=re.IGNORECASE)
            id_match = re.search(r"WHERE\s+id\s*=\s*(\d+)", sql, flags=re.IGNORECASE)

            if name_match:
                requested_name = name_match.group(1)
                output = (
                    f"SELECT 1\n{requested_name}"
                    if requested_name in self.state.inserted_names
                    else "SELECT 0"
                )
            elif id_match:
                requested_id = id_match.group(1)
                time.sleep(0.2)
                output = f"SELECT 1\nname_{requested_id}"
            elif "SELECT" in upper_sql:
                time.sleep(0.1)
                output = "SELECT 1\nname_1"

            affected_rows = 0 if output == "SELECT 0" else 1
            self._send_json(
                200,
                {
                    "ok": True,
                    "path": "/query",
                    "status_code": 200,
                    "message": f"SELECT {affected_rows}",
                    "affected_rows": affected_rows,
                    "output_text": output,
                },
            )
        finally:
            self.state.gate.release_read()

    def _handle_insert(self, sql: str) -> None:
        quoted_values = re.findall(r"'([^']*)'", sql)
        if len(quoted_values) < 2:
            self._send_json(
                400,
                {
                    "ok": False,
                    "path": "/query",
                    "status_code": 400,
                    "error_code": "query_failed",
                    "message": "invalid_insert_values",
                    "affected_rows": 0,
                    "output_text": "",
                },
            )
            return

        inserted_name = quoted_values[1]
        self.state.gate.acquire_write()
        try:
            time.sleep(0.12)
            self.state.inserted_names.add(inserted_name)
            self._send_json(
                200,
                {
                    "ok": True,
                    "path": "/query",
                    "status_code": 200,
                    "message": "INSERT 1",
                    "affected_rows": 1,
                    "output_text": "",
                },
            )
        finally:
            self.state.gate.release_write()


def case_file_paths_from_repo() -> list[Path]:
    cases_dir = Path(__file__).resolve().parents[1] / "cases"
    return sorted(cases_dir.glob("*.json"))


def run_self_test(timeout: float, print_response_bodies: bool) -> int:
    state = MockServerState()
    server = ThreadingHTTPServer(("127.0.0.1", 0), MockRequestHandler)
    server.state = state  # type: ignore[attr-defined]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base_url = f"http://127.0.0.1:{server.server_port}"

    try:
        reports = []
        all_ok = True
        for case_path in case_file_paths_from_repo():
            report = run_case_file(
                case_path=case_path,
                base_url_override=base_url,
                timeout=timeout,
                skip_health_check=False,
            )
            print_report(report, print_response_bodies)
            reports.append(report)
            all_ok = all_ok and report["ok"]
        return 0 if all_ok else 1
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=1.0)


def main() -> int:
    args = parse_args()

    if args.self_test:
        return run_self_test(args.timeout, args.print_response_bodies)

    if not args.case_paths:
        print("at least one case path or --self-test is required", file=sys.stderr)
        return 2

    reports = []
    all_ok = True

    for raw_case_path in args.case_paths:
        case_path = Path(raw_case_path)
        report = run_case_file(
            case_path=case_path,
            base_url_override=args.base_url,
            timeout=args.timeout,
            skip_health_check=args.skip_health_check,
        )
        print_report(report, args.print_response_bodies)
        reports.append(report)
        all_ok = all_ok and report["ok"]

    if args.output_json:
        output_path = Path(args.output_json)
        output_path.write_text(json.dumps({"cases": reports}, ensure_ascii=False, indent=2), encoding="utf-8")

    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
