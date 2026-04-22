# SERVER-RUNTIME scripts

- `run-server.sh`
  - `build/bin/db_server`를 기본 서버 바이너리로 보고 실행합니다.
  - `DB_SERVER_PORT`, `DB_SERVER_WORKERS`, `DB_SCHEMA_DIR`, `DB_DATA_DIR`를 기본값으로 연결합니다.
- `smoke-health.sh`
  - `GET /health` 스모크 체크를 위한 단순 `curl` 래퍼입니다.
- 루트 `compose.yaml`의 `server` 서비스는 내부에서 `make server && ./SERVER-RUNTIME/scripts/run-server.sh`를 실행합니다.
