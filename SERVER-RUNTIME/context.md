# SERVER-RUNTIME 컨텍스트

## 현재 상태
- 서버를 실제 프로세스로 묶는 런타임 전용 폴더다.
- `server_main` 공용 진입점과 런타임 설정 로더를 추가했다.
- 설정 우선순위는 `환경 변수 -> CLI override`가 아니라, 기본값 위에 환경 변수를 올리고 마지막에 CLI가 덮어쓰는 방식으로 정리했다.
- 기본 `schema/data` 경로는 repo root 기준 `week8-team7-db-api/schema`, `week8-team7-db-api/data`로 연결한다.
- `db_server_runtime.c` 기준으로 실제 listener, worker pool, HTTP/core wiring이 연결됐다.
- 루트 Docker 경로로 Linux 컨테이너에서 실행 가능하다.

## 다음 작업
- graceful shutdown 제어면과 live test 연결을 확장한다.
