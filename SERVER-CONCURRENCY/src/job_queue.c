#include "job_queue.h"

#include <stdlib.h>
#include <string.h>

static int job_is_valid(const ConcurrencyJob *job) {
    return job != NULL && job->run != NULL;
}

static void pop_front_locked(ConcurrencyJobQueue *queue, ConcurrencyJob *job_out) {
    *job_out = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;
}

int concurrency_job_queue_init(ConcurrencyJobQueue *queue, size_t capacity) {
    if (queue == NULL || capacity == 0) {
        return 0;
    }

    memset(queue, 0, sizeof(*queue));
    queue->items = (ConcurrencyJob *)calloc(capacity, sizeof(ConcurrencyJob));
    if (queue->items == NULL) {
        return 0;
    }

    queue->capacity = capacity;

    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue->items);
        memset(queue, 0, sizeof(*queue));
        return 0;
    }

    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        memset(queue, 0, sizeof(*queue));
        return 0;
    }

    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        memset(queue, 0, sizeof(*queue));
        return 0;
    }

    queue->ready = 1;
    return 1;
}

void concurrency_job_queue_close(ConcurrencyJobQueue *queue) {
    if (queue == NULL || !queue->ready) {
        return;
    }

    pthread_mutex_lock(&queue->mutex);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->not_full);
    pthread_cond_broadcast(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
}

ConcurrencyJobQueueStatus concurrency_job_queue_push(ConcurrencyJobQueue *queue, const ConcurrencyJob *job) {
    if (queue == NULL || !queue->ready || !job_is_valid(job)) {
        return CONCURRENCY_JOB_QUEUE_INVALID;
    }

    pthread_mutex_lock(&queue->mutex);
    while (queue->size == queue->capacity && !queue->closed) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return CONCURRENCY_JOB_QUEUE_CLOSED;
    }

    queue->items[queue->tail] = *job;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    return CONCURRENCY_JOB_QUEUE_OK;
}

ConcurrencyJobQueueStatus concurrency_job_queue_try_push(ConcurrencyJobQueue *queue, const ConcurrencyJob *job) {
    if (queue == NULL || !queue->ready || !job_is_valid(job)) {
        return CONCURRENCY_JOB_QUEUE_INVALID;
    }

    pthread_mutex_lock(&queue->mutex);

    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return CONCURRENCY_JOB_QUEUE_CLOSED;
    }

    if (queue->size == queue->capacity) {
        pthread_mutex_unlock(&queue->mutex);
        return CONCURRENCY_JOB_QUEUE_FULL;
    }

    queue->items[queue->tail] = *job;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    return CONCURRENCY_JOB_QUEUE_OK;
}

ConcurrencyJobQueueStatus concurrency_job_queue_pop(ConcurrencyJobQueue *queue, ConcurrencyJob *job_out) {
    if (queue == NULL || !queue->ready || job_out == NULL) {
        return CONCURRENCY_JOB_QUEUE_INVALID;
    }

    pthread_mutex_lock(&queue->mutex);
    while (queue->size == 0 && !queue->closed) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->size == 0 && queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return CONCURRENCY_JOB_QUEUE_CLOSED;
    }

    pop_front_locked(queue, job_out);
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return CONCURRENCY_JOB_QUEUE_OK;
}

size_t concurrency_job_queue_size(ConcurrencyJobQueue *queue) {
    size_t size;

    if (queue == NULL || !queue->ready) {
        return 0;
    }

    pthread_mutex_lock(&queue->mutex);
    size = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    return size;
}

int concurrency_job_queue_is_closed(ConcurrencyJobQueue *queue) {
    int closed;

    if (queue == NULL || !queue->ready) {
        return 1;
    }

    pthread_mutex_lock(&queue->mutex);
    closed = queue->closed;
    pthread_mutex_unlock(&queue->mutex);
    return closed;
}

size_t concurrency_job_queue_cancel_pending(ConcurrencyJobQueue *queue, ConcurrencyJobResult result) {
    ConcurrencyJob job;
    size_t cancelled;

    if (queue == NULL || !queue->ready) {
        return 0;
    }

    cancelled = 0;
    pthread_mutex_lock(&queue->mutex);
    while (queue->size > 0) {
        pop_front_locked(queue, &job);
        pthread_cond_signal(&queue->not_full);
        pthread_mutex_unlock(&queue->mutex);

        concurrency_job_finish(&job, result, -1, 0);
        cancelled++;

        pthread_mutex_lock(&queue->mutex);
    }
    pthread_mutex_unlock(&queue->mutex);
    return cancelled;
}

void concurrency_job_queue_destroy(ConcurrencyJobQueue *queue, ConcurrencyJobResult pending_result) {
    if (queue == NULL || !queue->ready) {
        return;
    }

    concurrency_job_queue_close(queue);
    concurrency_job_queue_cancel_pending(queue, pending_result);
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
    free(queue->items);
    memset(queue, 0, sizeof(*queue));
}

void concurrency_job_finish(
    ConcurrencyJob *job,
    ConcurrencyJobResult result,
    int worker_index,
    int run_status
) {
    if (job == NULL) {
        return;
    }

    if (job->notify != NULL) {
        job->notify(job->context, result, worker_index, run_status);
    }

    if (job->cleanup != NULL) {
        job->cleanup(job->context);
    }
}

const char *concurrency_job_result_name(ConcurrencyJobResult result) {
    switch (result) {
        case CONCURRENCY_JOB_RESULT_OK:
            return "ok";
        case CONCURRENCY_JOB_RESULT_FAILED:
            return "failed";
        case CONCURRENCY_JOB_RESULT_CANCELLED:
            return "cancelled";
        case CONCURRENCY_JOB_RESULT_REJECTED:
            return "rejected";
        default:
            return "unknown";
    }
}
