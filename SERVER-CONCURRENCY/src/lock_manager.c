#include "lock_manager.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int concurrency_thread_pool_current_worker_index(void);

#if defined(_WIN32)
#define LOCK_LOG_FLOCKFILE(stream) ((void)(stream))
#define LOCK_LOG_FUNLOCKFILE(stream) ((void)(stream))
#else
#define LOCK_LOG_FLOCKFILE(stream) flockfile(stream)
#define LOCK_LOG_FUNLOCKFILE(stream) funlockfile(stream)
#endif

static ConcurrencyLockMode effective_lock_mode(
    const ConcurrencyLockManager *manager,
    ConcurrencyLockMode mode
) {
    if (mode == CONCURRENCY_LOCK_MODE_NONE) {
        return CONCURRENCY_LOCK_MODE_NONE;
    }

    if (manager != NULL && manager->policy == CONCURRENCY_LOCK_POLICY_SERIAL_ALL) {
        return CONCURRENCY_LOCK_MODE_WRITE;
    }

    return mode;
}

static const char *skip_leading_sql_noise(const char *sql) {
    int skipped;

    if (sql == NULL) {
        return NULL;
    }

    do {
        skipped = 0;

        while (*sql != '\0' && isspace((unsigned char)*sql)) {
            sql++;
            skipped = 1;
        }

        if (sql[0] == '-' && sql[1] == '-') {
            sql += 2;
            skipped = 1;
            while (*sql != '\0' && *sql != '\n') {
                sql++;
            }
            continue;
        }

        if (sql[0] == '/' && sql[1] == '*') {
            sql += 2;
            skipped = 1;
            while (sql[0] != '\0' && !(sql[0] == '*' && sql[1] == '/')) {
                sql++;
            }

            if (sql[0] == '*' && sql[1] == '/') {
                sql += 2;
            }
        }
    } while (skipped);

    return sql;
}

static void log_lock_event(const ConcurrencyLockManager *manager,
                           const char *event,
                           ConcurrencyLockMode requested_mode,
                           ConcurrencyLockMode effective_mode) {
    LOCK_LOG_FLOCKFILE(stdout);
    fprintf(stdout,
            "[LOCK] | event=%s | worker=%d | requested=%s | effective=%s | active_readers=%zu | waiting_writers=%zu | writer_active=%d |\n",
            event,
            concurrency_thread_pool_current_worker_index(),
            concurrency_lock_mode_name(requested_mode),
            concurrency_lock_mode_name(effective_mode),
            manager == NULL ? 0u : manager->active_readers,
            manager == NULL ? 0u : manager->waiting_writers,
            manager == NULL ? 0 : manager->writer_active);
    LOCK_LOG_FUNLOCKFILE(stdout);
    fflush(stdout);
}

int concurrency_lock_manager_init(ConcurrencyLockManager *manager, ConcurrencyLockPolicy policy) {
    if (manager == NULL) {
        return 0;
    }

    memset(manager, 0, sizeof(*manager));
    manager->policy = policy;

    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        return 0;
    }

    if (pthread_cond_init(&manager->readers_cv, NULL) != 0) {
        pthread_mutex_destroy(&manager->mutex);
        memset(manager, 0, sizeof(*manager));
        return 0;
    }

    if (pthread_cond_init(&manager->writers_cv, NULL) != 0) {
        pthread_cond_destroy(&manager->readers_cv);
        pthread_mutex_destroy(&manager->mutex);
        memset(manager, 0, sizeof(*manager));
        return 0;
    }

    manager->ready = 1;
    return 1;
}

void concurrency_lock_manager_destroy(ConcurrencyLockManager *manager) {
    if (manager == NULL || !manager->ready) {
        return;
    }

    pthread_cond_destroy(&manager->writers_cv);
    pthread_cond_destroy(&manager->readers_cv);
    pthread_mutex_destroy(&manager->mutex);
    memset(manager, 0, sizeof(*manager));
}

void concurrency_lock_manager_acquire(ConcurrencyLockManager *manager, ConcurrencyLockMode mode) {
    ConcurrencyLockMode effective_mode;

    if (manager == NULL || !manager->ready) {
        return;
    }

    effective_mode = effective_lock_mode(manager, mode);
    if (effective_mode == CONCURRENCY_LOCK_MODE_NONE) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    if (effective_mode == CONCURRENCY_LOCK_MODE_READ) {
        while (manager->writer_active || manager->waiting_writers > 0) {
            log_lock_event(manager, "락 대기", mode, effective_mode);
            pthread_cond_wait(&manager->readers_cv, &manager->mutex);
        }

        manager->active_readers++;
        log_lock_event(manager, "락 획득", mode, effective_mode);
        pthread_mutex_unlock(&manager->mutex);
        return;
    }

    manager->waiting_writers++;
    while (manager->writer_active || manager->active_readers > 0) {
        log_lock_event(manager, "락 대기", mode, effective_mode);
        pthread_cond_wait(&manager->writers_cv, &manager->mutex);
    }

    manager->waiting_writers--;
    manager->writer_active = 1;
    log_lock_event(manager, "락 획득", mode, effective_mode);
    pthread_mutex_unlock(&manager->mutex);
}

void concurrency_lock_manager_release(ConcurrencyLockManager *manager, ConcurrencyLockMode mode) {
    ConcurrencyLockMode effective_mode;

    if (manager == NULL || !manager->ready) {
        return;
    }

    effective_mode = effective_lock_mode(manager, mode);
    if (effective_mode == CONCURRENCY_LOCK_MODE_NONE) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    if (effective_mode == CONCURRENCY_LOCK_MODE_READ) {
        if (manager->active_readers > 0) {
            manager->active_readers--;
        }

        log_lock_event(manager, "락 해제", mode, effective_mode);

        if (manager->active_readers == 0) {
            if (manager->waiting_writers > 0) {
                pthread_cond_signal(&manager->writers_cv);
            } else {
                pthread_cond_broadcast(&manager->readers_cv);
            }
        }

        pthread_mutex_unlock(&manager->mutex);
        return;
    }

    manager->writer_active = 0;
    log_lock_event(manager, "락 해제", mode, effective_mode);
    if (manager->waiting_writers > 0) {
        pthread_cond_signal(&manager->writers_cv);
    } else {
        pthread_cond_broadcast(&manager->readers_cv);
    }

    pthread_mutex_unlock(&manager->mutex);
}

ConcurrencyLockMode concurrency_lock_mode_from_sql(const char *sql) {
    char keyword[16];
    size_t index;

    if (sql == NULL) {
        return CONCURRENCY_LOCK_MODE_WRITE;
    }

    sql = skip_leading_sql_noise(sql);

    if (*sql == '\0') {
        return CONCURRENCY_LOCK_MODE_WRITE;
    }

    index = 0;
    while (*sql != '\0' && isalpha((unsigned char)*sql) && index + 1 < sizeof(keyword)) {
        keyword[index] = (char)toupper((unsigned char)*sql);
        index++;
        sql++;
    }
    keyword[index] = '\0';

    if (strcmp(keyword, "SELECT") == 0) {
        return CONCURRENCY_LOCK_MODE_READ;
    }

    return CONCURRENCY_LOCK_MODE_WRITE;
}

const char *concurrency_lock_mode_name(ConcurrencyLockMode mode) {
    switch (mode) {
        case CONCURRENCY_LOCK_MODE_NONE:
            return "none";
        case CONCURRENCY_LOCK_MODE_READ:
            return "read";
        case CONCURRENCY_LOCK_MODE_WRITE:
            return "write";
        default:
            return "unknown";
    }
}

const char *concurrency_lock_policy_name(ConcurrencyLockPolicy policy) {
    switch (policy) {
        case CONCURRENCY_LOCK_POLICY_READERS_PARALLEL:
            return "readers_parallel";
        case CONCURRENCY_LOCK_POLICY_SERIAL_ALL:
            return "serial_all";
        default:
            return "unknown";
    }
}
