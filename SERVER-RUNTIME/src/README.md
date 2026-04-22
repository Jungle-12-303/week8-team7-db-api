# SERVER-RUNTIME src

- `runtime_config.c`
  - CLI/env 우선순위에 따라 포트와 worker 수를 읽습니다.
  - 현재 작업 디렉터리 또는 실행 파일 경로를 기준으로 repo root를 추론합니다.
  - `week8-team7-db-api/schema`, `week8-team7-db-api/data` 기본 경로를 연결합니다.
- `server_main.c`
  - 런타임 설정을 검증하고 서버 구현체 훅을 호출하는 공용 진입점을 제공합니다.
- `db_server_runtime.c`
  - listen socket, accept loop, worker submit, HTTP dependency wiring, shutdown 경로를 구현합니다.
- `db_server_main.c`
  - `db_server` 바이너리의 실제 `main()` 엔트리입니다.
