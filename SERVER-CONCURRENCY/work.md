# SERVER-CONCURRENCY 진행 현황

## 진행중
- 공개 헤더와 기본 구현 추가 완료
- 통합 서버 경로에서 HTTP 작업 컨텍스트와 notify 콜백 연결 대기

## 업데이트 필요
- `SERVER-HTTP` 작업 컨텍스트 구조체와 notify 콜백 연결
- 실제 통합 서버 경로에서 동시성 테스트 케이스 실행
- `readers_parallel` 정책은 MVP 이후 최적화 트랙으로 분리 검토

## 완료
- 2026-04-22 KST
  - 루트 `PLAN.md` 기준 MVP 기본 락 정책을 `serial_all`로 정리함
  - SERVER-CONCURRENCY 템플릿 폴더와 기록 구조를 생성함
  - bounded blocking queue, fair lock manager, worker thread pool 구현을 추가함
  - graceful shutdown의 drain/cancel-pending 모드를 공개 인터페이스에 반영함
  - `gcc -Wall -Wextra -Werror -std=c11 -pthread` 정적 컴파일 검증과 스모크 테스트를 통과함
