# TEST-HTTP-FUNCTIONAL 진행 현황

## 진행중
- 현재 진행중인 로컬 작업 없음

## 업데이트 필요
- Windows 호스트 PowerShell에서 `http://127.0.0.1:8080` direct 접근이 timeout 나는 원인을 분리할 것
  그 전까지 canonical live 실행 경로는 `run-http-functional-tests.ps1 -Transport docker-compose-exec`로 둔다.
- 현재 즉시 범위는 동시성 러너 출력 개편이므로 HTTP 기능 트랙은 existing live 응답과 `[HTTP]` 요청/응답 로그 확인 수준만 유지하고 DB 단계 로그 검증은 후순위로 둔다

## 완료
- 2026-04-22 KST
  - `SERVER-RUNTIME/scripts/run-server.sh` 기준 live server 기동 경로를 확인함
  - `run-http-functional-tests.ps1`에 `docker-compose-exec` transport를 추가해 호스트 localhost 경로와 분리된 live smoke 실행 경로를 확보함
  - `run-http-functional-tests.ps1 -Transport docker-compose-exec -BaseUrl http://127.0.0.1:8080`로 12개 HTTP 기능 케이스를 실제 서버 기준 `12/12 passed`로 확인함
  - `/health` live 응답 `{"ok":true,"path":"/health","status_code":200,"message":"server is healthy"}`를 확인함
  - `/query` live 성공 응답 `{"ok":true,"path":"/query","status_code":200,"message":"SELECT 1",...}`와 SQL 실패 응답 `{"ok":false,"path":"/query","status_code":400,"error_code":"query_failed",...}`를 확인함
  - `X-Debug-Sleep-Ms: 300` 케이스의 live 응답 `200`과 약 `320ms` 지연을 확인함
  - `X-Debug-Sleep-Ms: 30000` 케이스의 live 응답 `400` + `error_code: "invalid_header"`를 확인함
  - `docker compose logs` 기준 요청 추적 로그 형식 `[HTTP] | req_id=... | event=요청 수신/응답 완료 | ... | status=... |`를 확인함
  - `/query` 요청 계약을 JSON `{ "sql": ... }`에서 raw SQL text body로 정리함
  - `/health`, `/query`, method/path/body 오류 케이스를 현재 `SERVER-HTTP` 필드(`ok`, `path`, `status_code`, `error_code`, `message`, `affected_rows`, `output_text`) 기준으로 갱신함
  - `run-http-functional-tests.ps1 -ValidateOnly`로 12개 케이스 정의 검증을 다시 통과함
  - TEST-HTTP-FUNCTIONAL 템플릿 폴더와 기록 구조를 생성함
  - `/health`, `/query`, 잘못된 method/path/body 검증용 케이스 JSON을 추가함
  - `cases/*.json` 기반 PowerShell HTTP 기능 러너를 추가함
  - `/query` 요청/응답 로컬 계약을 문서로 고정함
