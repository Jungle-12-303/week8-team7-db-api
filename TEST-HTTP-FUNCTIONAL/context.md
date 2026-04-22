# TEST-HTTP-FUNCTIONAL 컨텍스트

## 현재 상태
- HTTP 기능 테스트 전용 폴더다.
- `/health`, `/query`용 실케이스 JSON과 PowerShell 러너를 기준 산출물로 둔다.
- `docker-compose-exec` transport 기준 live endpoint 12개 케이스 통과를 확인했다.
- HTTP trace 로그는 `[HTTP] | req_id=... | event=요청 수신/응답 완료 | ... | status=... |` 형식을 사용한다.

## 다음 작업
- Windows 호스트 direct `localhost` timeout 원인을 분리한다.
- 런타임이 DB 단계 로그와 shutdown summary를 추가하면 기능 테스트 로그 검증 범위를 확장한다.

## 로컬 계약
- `POST /query` 요청 본문은 raw SQL text를 사용한다.
- `/health` 응답은 `ok`, `path`, `status_code`, `message`를 사용한다.
- `/query` 응답은 `ok`, `path`, `status_code`, `message`, `affected_rows`, `output_text`를 사용한다.
- SQL 논리 실패는 HTTP `400` + `ok: false` + `error_code: "query_failed"`로 구분한다.
- `X-Debug-Sleep-Ms`는 `0~10000ms` 범위만 허용하고 잘못된 값은 HTTP `400` + `error_code: "invalid_header"`로 처리한다.
