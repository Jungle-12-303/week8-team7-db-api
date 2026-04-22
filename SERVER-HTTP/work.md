# SERVER-HTTP 진행 현황

## 진행중
- `SERVER-CONCURRENCY`, `SERVER-RUNTIME`와 overload/queue saturation 시 HTTP 상태 코드 계약 정리
- live endpoint 결과 기준 `TEST-HTTP-FUNCTIONAL`, `TEST-EDGE-FAILURE` 케이스 추적

## 업데이트 필요
- `SERVER-CONCURRENCY`, `SERVER-RUNTIME` 경계에서 overload/queue saturation 응답을 `503`, `429`, 연결 종료 중 무엇으로 고정할지 문서화
- live endpoint 결과를 기준으로 `TEST-HTTP-FUNCTIONAL`, `TEST-EDGE-FAILURE` 케이스를 계속 동기화
- 필요하면 `X-Debug-Sleep-Ms` 동작을 기능 테스트나 데모 스크립트에 반영

## 완료
- 2026-04-22 KST
  - Postman 동시성 시연용 `X-Debug-Sleep-Ms` debug 헤더를 구현함
  - `X-Debug-Sleep-Ms`는 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 적용되도록 고정함
  - `X-Debug-Sleep-Ms` 범위를 `0~10000ms`로 제한하고, 잘못된 값은 `400 invalid_header`로 응답하도록 정리함
  - Docker 컨테이너에서 `make clean && make server` 재빌드를 통과해 `SERVER-HTTP` 변경분 컴파일을 확인함
  - `X-Debug-Sleep-Ms: 3000` 요청 실측 결과 약 `3.66초`로 지연 적용을 확인함
  - Docker 기반 `db_server` 런타임에서 `/health`, `/query` 기본 응답을 확인함
  - `http_server_handle_client()`가 실제 worker 기반 경로에 연결됨
  - `TEST-HTTP-FUNCTIONAL` 케이스 계약을 raw SQL body와 현재 JSON 응답 필드에 맞춰 정렬함
  - HTTP parser, router, JSON response 계약 구현을 완료함
  - 서버 런타임 진입점 wiring을 완료함
  - SERVER-HTTP 템플릿 헤더와 기록 구조를 생성함
  - `/query` 본문 형식을 raw SQL text로 확정함
  - `/health`, `/query`, 오류 응답 JSON 스키마를 구현함
