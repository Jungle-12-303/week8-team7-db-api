#include "thread_pool.h"

#include <stdlib.h>
#include <string.h>

struct ConcurrencyThreadPoolWorkerState {
    ConcurrencyThreadPool *pool;
    int worker_index;
};

static void *worker_main(void *arg) {
    ConcurrencyThreadPoolWorkerState *worker;
    ConcurrencyJob job;
    ConcurrencyJobQueueStatus status;
    int run_status;
    ConcurrencyJobResult result;

    worker = (ConcurrencyThreadPoolWorkerState *)arg;
    memset(&job, 0, sizeof(job));

    for (;;) {
        status = concurrency_job_queue_pop(&worker->pool->queue, &job);
        if (status == CONCURRENCY_JOB_QUEUE_CLOSED) {
            break;
        }

        if (status != CONCURRENCY_JOB_QUEUE_OK) {
            continue;
        }

        concurrency_lock_manager_acquire(&worker->pool->lock_manager, job.lock_mode);
        run_status = job.run(job.context);
        concurrency_lock_manager_release(&worker->pool->lock_manager, job.lock_mode);

        result = run_status == 0 ? CONCURRENCY_JOB_RESULT_OK : CONCURRENCY_JOB_RESULT_FAILED;
        concurrency_job_finish(&job, result, worker->worker_index, run_status);
        memset(&job, 0, sizeof(job));
    }

    return NULL;
}

static int thread_pool_accepting_jobs(ConcurrencyThreadPool *pool) {
    int accepting_jobs;

    pthread_mutex_lock(&pool->state_mutex);
    accepting_jobs = pool->accepting_jobs;
    pthread_mutex_unlock(&pool->state_mutex);
    return accepting_jobs;
}

int concurrency_thread_pool_init(
    ConcurrencyThreadPool *pool,
    size_t worker_count,
    size_t queue_capacity,
    ConcurrencyLockPolicy lock_policy
) {
    size_t index;

    if (pool == NULL || worker_count == 0 || queue_capacity == 0) {
        return 0;
    }

    memset(pool, 0, sizeof(*pool));

    if (pthread_mutex_init(&pool->state_mutex, NULL) != 0) {
        return 0;
    }
    pool->state_mutex_ready = 1;

    if (!concurrency_lock_manager_init(&pool->lock_manager, lock_policy)) {
        concurrency_thread_pool_destroy(pool);
        return 0;
    }
    pool->lock_manager_ready = 1;

    if (!concurrency_job_queue_init(&pool->queue, queue_capacity)) {
        concurrency_thread_pool_destroy(pool);
        return 0;
    }
    pool->queue_ready = 1;

    pool->workers = (pthread_t *)calloc(worker_count, sizeof(pthread_t));
    pool->worker_states = (ConcurrencyThreadPoolWorkerState *)calloc(
        worker_count,
        sizeof(ConcurrencyThreadPoolWorkerState)
    );
    if (pool->workers == NULL || pool->worker_states == NULL) {
        concurrency_thread_pool_destroy(pool);
        return 0;
    }

    pool->worker_count = worker_count;
    pool->accepting_jobs = 1;

    for (index = 0; index < worker_count; index++) {
        pool->worker_states[index].pool = pool;
        pool->worker_states[index].worker_index = (int)index;

        if (pthread_create(&pool->workers[index], NULL, worker_main, &pool->worker_states[index]) != 0) {
            pool->worker_count = index;
            concurrency_thread_pool_destroy(pool);
            return 0;
        }
    }

    return 1;
}

ConcurrencyJobQueueStatus concurrency_thread_pool_submit(
    ConcurrencyThreadPool *pool,
    const ConcurrencyJob *job
) {
    if (pool == NULL || !pool->state_mutex_ready || !thread_pool_accepting_jobs(pool)) {
        return CONCURRENCY_JOB_QUEUE_CLOSED;
    }

    return concurrency_job_queue_push(&pool->queue, job);
}

ConcurrencyJobQueueStatus concurrency_thread_pool_try_submit(
    ConcurrencyThreadPool *pool,
    const ConcurrencyJob *job
) {
    if (pool == NULL || !pool->state_mutex_ready || !thread_pool_accepting_jobs(pool)) {
        return CONCURRENCY_JOB_QUEUE_CLOSED;
    }

    return concurrency_job_queue_try_push(&pool->queue, job);
}

int concurrency_thread_pool_is_accepting(ConcurrencyThreadPool *pool) {
    int accepting_jobs;

    if (pool == NULL || !pool->state_mutex_ready) {
        return 0;
    }

    pthread_mutex_lock(&pool->state_mutex);
    accepting_jobs = pool->accepting_jobs;
    pthread_mutex_unlock(&pool->state_mutex);
    return accepting_jobs;
}

void concurrency_thread_pool_shutdown(
    ConcurrencyThreadPool *pool,
    ConcurrencyThreadPoolShutdownMode mode
) {
    size_t index;

    if (pool == NULL || !pool->state_mutex_ready) {
        return;
    }

    pthread_mutex_lock(&pool->state_mutex);
    if (pool->shutdown_started) {
        pthread_mutex_unlock(&pool->state_mutex);
        return;
    }

    pool->shutdown_started = 1;
    pool->accepting_jobs = 0;
    pthread_mutex_unlock(&pool->state_mutex);

    if (pool->queue_ready) {
        concurrency_job_queue_close(&pool->queue);
        if (mode == CONCURRENCY_THREAD_POOL_SHUTDOWN_CANCEL_PENDING) {
            concurrency_job_queue_cancel_pending(&pool->queue, CONCURRENCY_JOB_RESULT_CANCELLED);
        }
    }

    if (!pool->joined && pool->workers != NULL) {
        for (index = 0; index < pool->worker_count; index++) {
            pthread_join(pool->workers[index], NULL);
        }
        pool->joined = 1;
    }
}

void concurrency_thread_pool_destroy(ConcurrencyThreadPool *pool) {
    if (pool == NULL) {
        return;
    }

    if (pool->state_mutex_ready) {
        concurrency_thread_pool_shutdown(pool, CONCURRENCY_THREAD_POOL_SHUTDOWN_CANCEL_PENDING);
    }

    free(pool->worker_states);
    free(pool->workers);
    pool->worker_states = NULL;
    pool->workers = NULL;
    pool->worker_count = 0;

    if (pool->queue_ready) {
        concurrency_job_queue_destroy(&pool->queue, CONCURRENCY_JOB_RESULT_CANCELLED);
        pool->queue_ready = 0;
    }

    if (pool->lock_manager_ready) {
        concurrency_lock_manager_destroy(&pool->lock_manager);
        pool->lock_manager_ready = 0;
    }

    if (pool->state_mutex_ready) {
        pthread_mutex_destroy(&pool->state_mutex);
        pool->state_mutex_ready = 0;
    }

    pool->accepting_jobs = 0;
    pool->shutdown_started = 0;
    pool->joined = 0;
}

const char *concurrency_thread_pool_shutdown_mode_name(ConcurrencyThreadPoolShutdownMode mode) {
    switch (mode) {
        case CONCURRENCY_THREAD_POOL_SHUTDOWN_DRAIN:
            return "drain";
        case CONCURRENCY_THREAD_POOL_SHUTDOWN_CANCEL_PENDING:
            return "cancel_pending";
        default:
            return "unknown";
    }
}
