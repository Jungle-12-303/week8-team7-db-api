/*
 * job_queue.h
 *
 * worker pool이 공유하는 bounded blocking queue의 공개 헤더다.
 */
#ifndef SERVER_CONCURRENCY_JOB_QUEUE_H
#define SERVER_CONCURRENCY_JOB_QUEUE_H

#include "lock_manager.h"

#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONCURRENCY_JOB_RESULT_OK = 0,
    CONCURRENCY_JOB_RESULT_FAILED,
    CONCURRENCY_JOB_RESULT_CANCELLED,
    CONCURRENCY_JOB_RESULT_REJECTED
} ConcurrencyJobResult;

/* worker가 실제 작업을 수행하는 함수다. 0은 성공, 그 외 값은 실패다. */
typedef int (*ConcurrencyJobRunFn)(void *context);

/* 작업 종료 후 호출자에게 결과를 알리는 콜백이다. */
typedef void (*ConcurrencyJobNotifyFn)(
    void *context,
    ConcurrencyJobResult result,
    int worker_index,
    int run_status
);

/* notify 이후 context 정리에 사용할 선택적 콜백이다. */
typedef void (*ConcurrencyJobCleanupFn)(void *context);

/*
 * 하나의 작업 단위다.
 * lock_mode는 worker가 run 호출 전 lock manager에 적용한다.
 */
typedef struct {
    ConcurrencyLockMode lock_mode;
    ConcurrencyJobRunFn run;
    ConcurrencyJobNotifyFn notify;
    ConcurrencyJobCleanupFn cleanup;
    void *context;
} ConcurrencyJob;

typedef enum {
    CONCURRENCY_JOB_QUEUE_OK = 0,
    CONCURRENCY_JOB_QUEUE_FULL,
    CONCURRENCY_JOB_QUEUE_EMPTY,
    CONCURRENCY_JOB_QUEUE_CLOSED,
    CONCURRENCY_JOB_QUEUE_INVALID
} ConcurrencyJobQueueStatus;

typedef struct {
    ConcurrencyJob *items;
    size_t capacity;
    size_t size;
    size_t head;
    size_t tail;
    int closed;
    int ready;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ConcurrencyJobQueue;

/* bounded queue를 초기화한다. capacity는 1 이상이어야 한다. */
int concurrency_job_queue_init(ConcurrencyJobQueue *queue, size_t capacity);

/* 새 작업 수락을 중단하고 대기 중인 producer/worker를 깨운다. */
void concurrency_job_queue_close(ConcurrencyJobQueue *queue);

/* 공간이 생길 때까지 기다렸다가 작업을 넣는다. */
ConcurrencyJobQueueStatus concurrency_job_queue_push(ConcurrencyJobQueue *queue, const ConcurrencyJob *job);

/* queue가 가득 차 있으면 즉시 FULL을 반환한다. */
ConcurrencyJobQueueStatus concurrency_job_queue_try_push(ConcurrencyJobQueue *queue, const ConcurrencyJob *job);

/* 닫힌 queue가 비워질 때까지 worker가 blocking pop을 수행한다. */
ConcurrencyJobQueueStatus concurrency_job_queue_pop(ConcurrencyJobQueue *queue, ConcurrencyJob *job_out);

/* 현재 대기 중인 작업 수를 스냅샷으로 반환한다. */
size_t concurrency_job_queue_size(ConcurrencyJobQueue *queue);

/* queue가 새 작업을 받는지 여부를 확인한다. */
int concurrency_job_queue_is_closed(ConcurrencyJobQueue *queue);

/* 대기 중인 작업들을 모두 꺼내 취소/거절 결과로 정리한다. */
size_t concurrency_job_queue_cancel_pending(ConcurrencyJobQueue *queue, ConcurrencyJobResult result);

/*
 * 남아 있는 작업을 지정한 결과 코드로 정리하고 내부 자원을 해제한다.
 * notify가 필요 없으면 cleanup만 두면 된다.
 */
void concurrency_job_queue_destroy(ConcurrencyJobQueue *queue, ConcurrencyJobResult pending_result);

/* notify 후 cleanup 순서로 하나의 작업 생명주기를 마무리한다. */
void concurrency_job_finish(ConcurrencyJob *job, ConcurrencyJobResult result, int worker_index, int run_status);

/* 디버깅과 로그를 위해 결과 이름을 돌려준다. */
const char *concurrency_job_result_name(ConcurrencyJobResult result);

#ifdef __cplusplus
}
#endif

#endif
