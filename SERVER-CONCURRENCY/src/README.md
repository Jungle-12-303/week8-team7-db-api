# SERVER-CONCURRENCY src

- worker thread, queue, `read-read 병렬 / write-* 직렬` 정책 구현 파일을 둡니다.
- 현재 구현 파일
  - `thread_pool.c`
  - `job_queue.c`
  - `lock_manager.c`
