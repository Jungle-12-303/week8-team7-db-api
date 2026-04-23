# SERVER-HTTP 컨텍스트

## 현재 상태
- HTTP 계층은 엔진을 직접 모르고 `SERVER-CORE`를 통해서만 호출한다.
- `/query` 본문 포맷은 raw SQL text로 고정했다.
  - 허용 `Content-Type`: 없음, `text/plain`, `application/sql`
- 공통 JSON 기본 필드는 `ok`, `path`, `status_code`, `message`로 고정했다.
- per-request `http_request_trace`를 통해 runtime이 할당한 req_id와 worker index를 받아 로그에 재사용한다.
- 현재 `[HTTP]` 로그는 `thread=<worker_index>`까지 포함한다.

## 다음 작업
- `SERVER-CORE` 실행 결과 구조체와 콜백 연결 규칙을 맞춘다.
- 기능 테스트 트랙에서 `/health`, `/query` 케이스를 확정한다.
