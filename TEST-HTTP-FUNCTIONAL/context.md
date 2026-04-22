# TEST-HTTP-FUNCTIONAL 컨텍스트

## 현재 상태
- HTTP 기능 테스트 전용 폴더다.
- `/health`, `/query`용 실케이스 JSON과 PowerShell 러너를 기준 산출물로 둔다.

## 다음 작업
- 서버 실행 경로가 연결되면 실제 HTTP smoke 실행 결과를 누적한다.

## 로컬 계약
- `POST /query` 요청 본문은 raw SQL text를 사용한다.
- `/health` 응답은 `ok`, `path`, `status_code`, `message`를 사용한다.
- `/query` 응답은 `ok`, `path`, `status_code`, `message`, `affected_rows`, `output_text`를 사용한다.
- SQL 논리 실패는 HTTP `400` + `ok: false` + `error_code: "query_failed"`로 구분한다.
