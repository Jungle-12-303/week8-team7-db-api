# SERVER-RUNTIME include

- `server_runtime/runtime_config.h`
  - 포트, worker 수, repo root, schema/data 경로를 담는 런타임 설정 구조체를 정의합니다.
  - CLI와 환경 변수에서 설정을 읽는 함수와 usage 출력 함수를 제공합니다.
- `server_runtime/server_main.h`
  - 실제 서버 구현체를 붙일 수 있도록 `start`, `wait`, `request_stop` 훅 기반 진입점을 정의합니다.
