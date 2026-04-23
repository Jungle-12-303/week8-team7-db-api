# SERVER-CONCURRENCY 이슈

## 열린 이슈
- 현재 기록된 로컬 이슈 없음

## 해결됨
- 2026-04-23 KST
  - `SERVER-HTTP`와 `SERVER-RUNTIME` 사이에 req_id/worker/status를 공유하는 trace 경계를 추가해 worker 할당 로그가 요청 로그와 끊기지 않도록 정리함
  - `src/lock_manager.c`에 고정 형식 `[LOCK]` 로그와 `락 대기/획득/해제` 이벤트를 추가해 `serial_all`에서 read 요청이 write로 승격되는 상태를 관측 가능하게 함
  - rejection 경로에서 이미 닫은 client fd를 cleanup이 다시 닫을 수 있던 double-close 위험을 제거함
  - Docker live runtime과 `TEST-CONCURRENCY` 4케이스 기준으로 새 로그 배선이 기능 회귀 없이 동작함을 확인함
- 2026-04-22 KST
  - 기본 락 정책 문서가 루트 `PLAN.md`와 어긋나지 않도록 `serial_all` 기준을 다시 고정함
  - 동시성 정책 전용 폴더 구조를 분리함
