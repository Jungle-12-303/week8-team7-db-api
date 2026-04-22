# TEST-HTTP-FUNCTIONAL cases

- `/health`, `/query`, 잘못된 method/path/body, JSON 응답 구조 테스트 케이스를 둡니다.
- 케이스 파일은 `*.json` 형식이며 `request`와 `expect` 섹션으로 나눕니다.
- 로컬 계약은 아래를 기준으로 고정합니다.
  - `GET /health`는 `200`과 JSON `{ ok, path, status_code, message }`를 반환한다.
  - `POST /query`는 raw SQL text body를 입력으로 받는다.
  - 허용 `Content-Type`은 없음, `text/plain`, `application/sql`이다.
  - optional debug header `X-Debug-Sleep-Ms`는 `0~10000ms` 범위에서만 허용한다.
  - 잘못된 `X-Debug-Sleep-Ms`는 HTTP `400` + `error_code: "invalid_header"`로 처리한다.
  - `/query` 응답은 `ok`, `path`, `status_code`, `message`, `affected_rows`, `output_text`를 사용한다.
  - SQL 실행 실패는 HTTP `400` + `ok: false` + `error_code: "query_failed"`로 표현한다.
  - 잘못된 method/path/body는 각각 `405`, `404`, `400`/`415`로 처리한다.
- 예시: `01-health-ok.json`, `07-query-sql-error.json`, `11-query-debug-sleep-ok.json`, `12-query-invalid-debug-sleep-header.json`
