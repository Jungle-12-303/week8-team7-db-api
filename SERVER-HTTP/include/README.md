# SERVER-HTTP include

- `http_protocol.h`
  - HTTP/1.0, HTTP/1.1 request completeness detection
  - request parsing
  - JSON response builders
- `http_server.h`
  - single-client connection handling
  - `/health`, `/query` routing
  - `SERVER-CORE` query execution callback contract
