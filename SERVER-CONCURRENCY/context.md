# SERVER-CONCURRENCY 컨텍스트

## 현재 상태
- 동시성 정책과 실행 스케줄링을 전담하는 폴더다.
- `thread_pool`, `job_queue`, `lock_manager` 공개 인터페이스와 기본 구현이 추가되었다.
- lock manager는 `read-read 병렬 / write-* 직렬` 정책과 루트 계획 호환용 `serial-all` 정책을 모두 지원한다.
- worker가 job을 꺼낸 직후 `assigned` 콜백을 호출하고, 현재 worker index를 thread-local accessor로 노출한다.
- `gcc -Wall -Wextra -Werror -std=c11 -pthread` 기준 정적 컴파일 검증과 간단한 런타임 스모크 테스트를 통과했다.
- `SERVER-HTTP`의 per-request trace와 `SERVER-RUNTIME` job context가 연결되어 req_id 기준 `스레드 할당`, `작업 종료`, `thread=<worker_index>` 로그를 남긴다.
- lock manager는 `[LOCK]` 고정 형식 로그로 `락 대기/획득/해제`, `requested/effective` 모드, 현재 reader/writer 상태를 직접 출력한다.
- Docker live runtime 기준 `TEST-CONCURRENCY` 4개 케이스를 재실행해 PASS를 확인했고 server logs에서 새 로그 포맷을 검증했다.

## 다음 작업
- `SERVER-CORE` 연동 시 MVP 기본값은 `serial_all`로 두고, `readers_parallel`은 후속 최적화 트랙으로 분리한다.
- runtime 요약 로그와 `DB 작업 시작/종료` 이벤트는 별도 로그 확장 트랙에서 정리한다.
