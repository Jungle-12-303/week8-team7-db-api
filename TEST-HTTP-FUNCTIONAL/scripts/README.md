# TEST-HTTP-FUNCTIONAL scripts

- `cases/*.json`을 읽어 HTTP 기능 케이스를 실행하는 스크립트를 둡니다.
- 기본 러너는 `run-http-functional-tests.ps1`이며 상태 코드, Content-Type, JSON 필드/타입/값, 선택적으로 응답 시간을 검증합니다.
- transport
  - `direct`: 호스트에서 대상 URL로 직접 요청합니다.
  - `docker-compose-exec`: `docker compose exec`로 컨테이너 내부 `curl`을 사용해 live endpoint를 검증합니다.
- 예시 실행:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\run-http-functional-tests.ps1 -BaseUrl http://127.0.0.1:8080`
  - `powershell -ExecutionPolicy Bypass -File .\scripts\run-http-functional-tests.ps1 -Transport docker-compose-exec -BaseUrl http://127.0.0.1:8080`
  - `powershell -ExecutionPolicy Bypass -File .\scripts\run-http-functional-tests.ps1 -ValidateOnly`
