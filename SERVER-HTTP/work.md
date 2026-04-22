# SERVER-HTTP 진행 현황

## 진행중
- `SERVER-CONCURRENCY`, `SERVER-RUNTIME`와 overload/queue saturation 시 HTTP 상태 코드 계약 정리
- live endpoint 결과 기준 `TEST-HTTP-FUNCTIONAL`, `TEST-EDGE-FAILURE` 케이스 추적
- Postman 동시성 시연용 `X-Debug-Sleep-Ms` debug 헤더 설계 검토

## 업데이트 필요
- `SERVER-CONCURRENCY`, `SERVER-RUNTIME` 경계에서 overload/queue saturation 응답을 `503`, `429`, 연결 종료 중 무엇으로 고정할지 문서화
- live endpoint 결과를 기준으로 `TEST-HTTP-FUNCTIONAL`, `TEST-EDGE-FAILURE` 케이스를 계속 동기화
- Postman 동시성 시연용 debug delay 기능 추가
  `X-Debug-Sleep-Ms` 헤더를 `SERVER-HTTP`의 `/query` 처리 직전에서 파싱하고, `execute_query(...)` 호출 전에 sleep을 적용할 것

주의점:
- SQL 파서나 DB 실행 로직은 건드리지 말고, HTTP 계층에서만 동작하도록 제한할 것
- sleep은 DB lock 구간 안이 아니라 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 적용할 것
- 기본값은 비활성으로 두고, 필요 시에만 켤 수 있는 debug 성격의 기능으로 유지할 것
- 입력값은 `0~10000ms` 범위로 제한하고, 범위를 벗어나면 무시하거나 명확한 오류로 처리할 것

## 완료
- 2026-04-22 KST
  - Docker 기반 `db_server` 런타임에서 `/health`, `/query` 기본 응답을 확인함
  - `http_server_handle_client()`가 실제 worker 기반 경로에 연결됨
  - `TEST-HTTP-FUNCTIONAL` 케이스 계약을 raw SQL body와 현재 JSON 응답 필드에 맞춰 정렬함
  - HTTP parser, router, JSON response 계약 구현을 완료함
  - 서버 런타임 진입점 wiring을 완료함
  - SERVER-HTTP 템플릿 헤더와 기록 구조를 생성함
  - `/query` 본문 형식을 raw SQL text로 확정함
  - `/health`, `/query`, 오류 응답 JSON 스키마를 구현함
