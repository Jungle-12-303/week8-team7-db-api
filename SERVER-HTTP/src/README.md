# SERVER-HTTP src

## Implemented files
- `http_protocol.c`
  - request completeness detection via `Content-Length`
  - request line and header parsing
  - JSON response rendering
- `http_server.c`
  - single-request connection lifecycle
  - `/health`, `/query` routing
  - query execution callback wiring

## HTTP contract

### Connection policy
- One TCP connection handles one HTTP request.
- The server always responds with `Connection: close`.
- `Transfer-Encoding: chunked` is not supported in this MVP.

### `GET /health`
- Request body must be empty.
- Success response:

```json
{
  "ok": true,
  "path": "/health",
  "status_code": 200,
  "message": "server is healthy"
}
```

### `POST /query`
- The request body is the raw SQL string.
- Accepted `Content-Type`
  - omitted
  - `text/plain`
  - `application/sql`
- Optional debug header
  - `X-Debug-Sleep-Ms: <0-10000>`
  - absent by default
  - applies only before `execute_query(...)`
  - intended for demo/testing traffic only
- Success response:

```json
{
  "ok": true,
  "path": "/query",
  "status_code": 200,
  "message": "SELECT 1",
  "affected_rows": 1,
  "output_text": "id,name\n1,Alice\n"
}
```

- SQL execution failure response:

```json
{
  "ok": false,
  "path": "/query",
  "status_code": 400,
  "error_code": "query_failed",
  "message": "missing SQL statement",
  "affected_rows": 0,
  "output_text": ""
}
```

## Error policy
- `400 Bad Request`
  - malformed request line or headers
  - blank `/query` body
  - body sent to `/health`
  - invalid `X-Debug-Sleep-Ms` header
- `404 Not Found`
  - unsupported path
- `405 Method Not Allowed`
  - wrong method for `/health` or `/query`
- `413 Payload Too Large`
  - request exceeds `max_request_bytes`
- `415 Unsupported Media Type`
  - `/query` body is not raw SQL content
- `500 Internal Server Error`
  - query executor callback is missing
  - `SERVER-CORE` bridge reports an infrastructure failure
