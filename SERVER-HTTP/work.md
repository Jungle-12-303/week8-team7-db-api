# SERVER-HTTP 진행 현황

## 진행중
- HTTP parser, router, JSON response 계층 구현 완료
- 런타임 진입점 wiring 완료

## 업데이트 필요
- live endpoint 기준 기능/엣지 테스트 누적
- 필요 시 overload 응답 세분화

## 완료
- 2026-04-22 KST
  - Docker 기반 `db_server` 런타임에서 `/health`, `/query` 내부 스모크 응답을 확인함
  - `http_server_handle_client()`가 실제 worker 런타임 경로에 연결됨
  - `TEST-HTTP-FUNCTIONAL` 케이스 계약을 raw SQL body와 현재 JSON 응답 필드에 맞춰 정렬함
  - SERVER-HTTP 템플릿 폴더와 기록 구조를 생성함
  - `/query` 본문 포맷을 raw SQL text로 확정함
  - `/health`, `/query`, 오류 응답 JSON 스키마를 구현함
