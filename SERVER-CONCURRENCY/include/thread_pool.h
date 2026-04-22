/*
 * thread_pool.h
 *
 * bounded queue와 lock manager를 묶어 worker lifecycle을 관리하는 공개 헤더다.
 */
#ifndef SERVER_CONCURRENCY_THREAD_POOL_H
#define SERVER_CONCURRENCY_THREAD_POOL_H

#include "job_queue.h"
#include "lock_manager.h"

#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONCURRENCY_THREAD_POOL_SHUTDOWN_DRAIN = 0,
    CONCURRENCY_THREAD_POOL_SHUTDOWN_CANCEL_PENDING
} ConcurrencyThreadPoolShutdownMode;

typedef struct ConcurrencyThreadPoolWorkerState ConcurrencyThreadPoolWorkerState;

typedef struct {
    size_t worker_count;
    pthread_t *workers;
    ConcurrencyThreadPoolWorkerState *worker_states;
    ConcurrencyJobQueue queue;
    ConcurrencyLockManager lock_manager;
    int accepting_jobs;
    int shutdown_started;
    int joined;
    int queue_ready;
    int lock_manager_ready;
    int state_mutex_ready;
    pthread_mutex_t state_mutex;
} ConcurrencyThreadPool;

/*
 * worker_count개의 worker와 bounded queue를 만들고 즉시 실행 상태로 전환한다.
 * lock_policy는 SQL 작업 락 정책의 기본 동작을 결정한다.
 */
int concurrency_thread_pool_init(
    ConcurrencyThreadPool *pool,
    size_t worker_count,
    size_t queue_capacity,
    ConcurrencyLockPolicy lock_policy
);

/* queue가 가득 차 있으면 공간이 생길 때까지 기다린다. */
ConcurrencyJobQueueStatus concurrency_thread_pool_submit(
    ConcurrencyThreadPool *pool,
    const ConcurrencyJob *job
);

/* queue가 가득 차 있으면 FULL을 즉시 반환한다. */
ConcurrencyJobQueueStatus concurrency_thread_pool_try_submit(
    ConcurrencyThreadPool *pool,
    const ConcurrencyJob *job
);

/* shutdown이 시작되기 전이면 1, 아니면 0을 반환한다. */
int concurrency_thread_pool_is_accepting(ConcurrencyThreadPool *pool);

/*
 * graceful shutdown을 수행한다.
 * DRAIN은 queue를 닫고 남은 작업을 모두 수행한 뒤 종료한다.
 * CANCEL_PENDING은 아직 시작되지 않은 작업을 취소하고 실행 중 작업만 마친 뒤 종료한다.
 */
void concurrency_thread_pool_shutdown(
    ConcurrencyThreadPool *pool,
    ConcurrencyThreadPoolShutdownMode mode
);

/*
 * shutdown 이후 남은 내부 자원을 정리한다.
 * shutdown을 호출하지 않았더라도 안전하게 CANCEL_PENDING 모드 종료 후 정리한다.
 */
void concurrency_thread_pool_destroy(ConcurrencyThreadPool *pool);

/*
 * 현재 스레드가 worker라면 worker index를, 아니면 -1을 반환한다.
 * HTTP/런타임 계층이 요청 로그에 thread 식별자를 붙일 때 사용한다.
 */
int concurrency_thread_pool_current_worker_index(void);

/* 디버깅과 로그를 위해 shutdown 모드 이름을 돌려준다. */
const char *concurrency_thread_pool_shutdown_mode_name(ConcurrencyThreadPoolShutdownMode mode);

#ifdef __cplusplus
}
#endif

#endif
