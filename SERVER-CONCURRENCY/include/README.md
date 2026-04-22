# SERVER-CONCURRENCY include

- thread pool, job queue, lock manager 공개 헤더를 둡니다.
- 현재 공개 헤더
  - `thread_pool.h`: worker lifecycle, 작업 제출, graceful shutdown
  - `job_queue.h`: bounded blocking queue, close/drain/cancel semantics
  - `lock_manager.h`: `read-read 병렬 / write-* 직렬` 또는 `serial-all` 정책
- 상위 `PLAN.md`의 보수적 기본값을 위해 통합 시에는 `CONCURRENCY_LOCK_POLICY_SERIAL_ALL`을 기본으로 둘 수 있다.
