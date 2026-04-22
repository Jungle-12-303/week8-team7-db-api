#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import signal
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


MAX_DEBUG_SLEEP_MS = 10000


class ServerState:
    def __init__(self, workers: int, queue_capacity: int, sql_bytes_limit: int, queue_wait_ms: int) -> None:
        self.sql_bytes_limit = sql_bytes_limit
        self.queue_wait_ms = queue_wait_ms
        self.draining = False
        self.total_slots = threading.BoundedSemaphore(workers + queue_capacity)
        self.worker_slots = threading.BoundedSemaphore(workers)


class MockEdgeHandler(BaseHTTPRequestHandler):
    server_version = "MockEdgeServer/0.1"

    @property
    def state(self) -> ServerState:
        return self.server.state  # type: ignore[attr-defined]

    def log_message(self, format: str, *args: object) -> None:
        return

    def safe_json(self, status: int, payload: dict[str, object]) -> None:
        normalized = dict(payload)
        normalized.setdefault("status_code", status)
        if status >= 400:
            normalized.setdefault("ok", False)
            if "error" in normalized and "error_code" not in normalized:
                normalized["error_code"] = normalized["error"]
            normalized.setdefault("message", str(normalized.get("error_code", "request_failed")))
        else:
            normalized.setdefault("ok", True)
        body = json.dumps(normalized, ensure_ascii=False).encode("utf-8")
        try:
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except OSError:
            pass

    def parse_debug_sleep_ms(self) -> int | None:
        raw_value = self.headers.get("X-Debug-Sleep-Ms")
        if raw_value is None:
            return 0
        try:
            sleep_ms = int(raw_value)
        except ValueError:
            self.safe_json(
                HTTPStatus.BAD_REQUEST,
                {
                    "error": "invalid_header",
                    "message": f"X-Debug-Sleep-Ms must be an integer between 0 and {MAX_DEBUG_SLEEP_MS}",
                },
            )
            return None
        if sleep_ms < 0 or sleep_ms > MAX_DEBUG_SLEEP_MS:
            self.safe_json(
                HTTPStatus.BAD_REQUEST,
                {
                    "error": "invalid_header",
                    "message": f"X-Debug-Sleep-Ms must be between 0 and {MAX_DEBUG_SLEEP_MS} milliseconds",
                },
            )
            return None
        return sleep_ms

    def do_GET(self) -> None:
        if self.path != "/health":
            self.safe_json(HTTPStatus.NOT_FOUND, {"error": "not_found"})
            return
        if self.state.draining:
            self.safe_json(HTTPStatus.SERVICE_UNAVAILABLE, {"error": "draining"})
            return
        self.safe_json(HTTPStatus.OK, {"ok": True})

    def do_POST(self) -> None:
        if self.path != "/query":
            self.safe_json(HTTPStatus.NOT_FOUND, {"error": "not_found"})
            return
        if self.state.draining:
            self.safe_json(HTTPStatus.SERVICE_UNAVAILABLE, {"error": "draining"})
            return
        if not self.state.total_slots.acquire(blocking=False):
            self.safe_json(HTTPStatus.SERVICE_UNAVAILABLE, {"error": "queue_overflow"})
            return
        try:
            self.handle_query()
        finally:
            self.state.total_slots.release()

    def handle_query(self) -> None:
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.safe_json(HTTPStatus.BAD_REQUEST, {"error": "invalid_content_length"})
            return

        try:
            body = self.rfile.read(content_length) if content_length > 0 else b""
        except OSError:
            return
        if len(body) < content_length:
            return
        if content_length == 0 or not body.strip():
            self.safe_json(HTTPStatus.BAD_REQUEST, {"error": "empty_body"})
            return
        if content_length > self.state.sql_bytes_limit:
            self.safe_json(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, {"error": "sql_too_large"})
            return

        acquired_worker = self.state.worker_slots.acquire(timeout=self.state.queue_wait_ms / 1000.0)
        if not acquired_worker:
            self.safe_json(HTTPStatus.SERVICE_UNAVAILABLE, {"error": "worker_exhausted"})
            return
        try:
            self.execute_query(body)
        finally:
            self.state.worker_slots.release()

    def execute_query(self, body: bytes) -> None:
        sql = body.decode("utf-8", errors="replace").strip()
        debug_sleep_ms = self.parse_debug_sleep_ms()
        if debug_sleep_ms is None:
            return
        sleep_ms = debug_sleep_ms or int(self.headers.get("X-Mock-Sleep-Ms", "0") or "0")
        if sleep_ms > 0:
            time.sleep(sleep_ms / 1000.0)
        if self.headers.get("X-Mock-Engine-Failure") == "1" or "ENGINE_INTERNAL_ERROR" in sql:
            self.safe_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": "engine_internal_error"})
            return
        if "SELECT FROM ;" in sql or "INVALID" in sql or not sql.endswith(";"):
            self.safe_json(HTTPStatus.BAD_REQUEST, {"error": "invalid_sql"})
            return
        self.safe_json(HTTPStatus.OK, {"ok": True, "message": "mock success", "rows": []})


class MockEdgeServer(ThreadingHTTPServer):
    request_queue_size = 128


def install_signal_handlers(server: ThreadingHTTPServer, state: ServerState) -> None:
    def handle_shutdown(_signum: int, _frame: object) -> None:
        state.draining = True
        threading.Thread(target=server.shutdown, daemon=True).start()

    for signum_name in ("SIGINT", "SIGTERM"):
        signum = getattr(signal, signum_name, None)
        if signum is not None:
            signal.signal(signum, handle_shutdown)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Mock server for TEST-EDGE-FAILURE.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18080)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--queue-capacity", type=int, default=2)
    parser.add_argument("--sql-bytes-limit", type=int, default=2048)
    parser.add_argument("--queue-wait-ms", type=int, default=100)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    state = ServerState(
        workers=args.workers,
        queue_capacity=args.queue_capacity,
        sql_bytes_limit=args.sql_bytes_limit,
        queue_wait_ms=args.queue_wait_ms,
    )
    server = MockEdgeServer((args.host, args.port), MockEdgeHandler)
    server.state = state  # type: ignore[attr-defined]
    install_signal_handlers(server, state)
    try:
        server.serve_forever()
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
