# SERVER-CONCURRENCY 진행 현황

## 진행중
- 현재 진행중인 로컬 작업 없음

## 업데이트 필요
- `readers_parallel` 정책은 MVP 이후 최적화 트랙으로 분리 검토
- runtime 요약 로그와 `DB 작업 시작/종료` 이벤트는 후속 로그 확장 트랙에서 정리

## 완료
- 2026-04-23 KST
  - `SERVER-HTTP` per-request trace와 `SERVER-RUNTIME` job context를 연결해 req_id 기준 `스레드 할당`, `작업 종료`, `thread=<worker_index>` 로그 흐름을 정리함
  - `[HTTP]` 로그에 `thread=<worker_index>` 필드를 반영하고, `assigned`/`notify` 콜백을 live runtime 경로에 연결함
  - `src/lock_manager.c`에 `[LOCK]` 고정 형식 로그와 `락 대기`, `락 획득`, `락 해제` 상태 전이 로그를 추가함
  - `serial_all` 정책에서 `requested=read`, `effective=write` 승격이 실제 server logs에 드러나는 것을 확인함
  - rejection 경로에서 닫힌 client fd를 cleanup이 다시 닫지 않도록 double-close 위험을 제거함
  - Docker live runtime 기준 `TEST-CONCURRENCY` 4개 케이스를 재실행해 PASS를 확인함
  - Docker server logs 기준 `[HTTP]`, `[RUNTIME]`, `[LOCK]` 로그 형식과 `thread=...`, `requested/effective`, `락 대기` 출력이 실제로 남는 것을 확인함
- 2026-04-22 KST
  - 루트 `PLAN.md` 기준 MVP 기본 락 정책을 `serial_all`로 정리함
  - SERVER-CONCURRENCY 템플릿 폴더와 기록 구조를 생성함
  - bounded blocking queue, fair lock manager, worker thread pool 구현을 추가함
  - graceful shutdown의 drain/cancel-pending 모드를 공개 인터페이스에 반영함
  - `gcc -Wall -Wextra -Werror -std=c11 -pthread` 정적 컴파일 검증과 스모크 테스트를 통과함
  - job dequeue 직후 실행되는 `assigned` 콜백을 추가해 req_id 기반 `스레드 할당` 로그 연결 지점을 마련함
  - thread-local worker index accessor `concurrency_thread_pool_current_worker_index()`를 추가해 상위 계층 로그에서 `thread=<worker_index>`를 노출할 수 있게 함
