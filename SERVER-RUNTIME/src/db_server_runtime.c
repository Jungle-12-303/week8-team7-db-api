#include "server_runtime/db_server_runtime.h"

#include "http_protocol.h"
#include "week8_engine.h"

#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_RUNTIME_LISTEN_BACKLOG 128
#define SERVER_RUNTIME_QUEUE_CAPACITY_FACTOR 4u
#define SERVER_RUNTIME_MIN_QUEUE_CAPACITY 8u

#ifdef MSG_NOSIGNAL
#define SERVER_RUNTIME_SEND_FLAGS MSG_NOSIGNAL
#else
#define SERVER_RUNTIME_SEND_FLAGS 0
#endif

typedef struct {
    DbServerRuntime *runtime;
    int client_fd;
    unsigned long long request_id;
} DbServerClientJob;

static DbServerRuntime *g_signal_runtime = NULL;
static _Thread_local unsigned long long g_runtime_request_id = 0;
static atomic_ullong g_runtime_next_request_id = 1;
static atomic_uint g_runtime_active_workers = 0;

static void runtime_log_event(unsigned long long request_id, const char *event, const char *details)
{
    fprintf(stdout, "component=runtime event=%s", event == NULL ? "unknown" : event);
    if (request_id != 0ULL) {
        fprintf(stdout, " request_id=%llu", request_id);
    }
    if (details != NULL && details[0] != '\0') {
        fprintf(stdout, " %s", details);
    }
    fputc('\n', stdout);
    fflush(stdout);
}

static void runtime_log_lock_event(
    unsigned long long request_id,
    const char *event,
    ConcurrencyLockManager *manager,
    ConcurrencyLockMode mode)
{
    char details[256];
    size_t active_readers = 0;
    size_t waiting_writers = 0;
    int writer_active = 0;

    if (manager != NULL && manager->ready) {
        pthread_mutex_lock(&manager->mutex);
        active_readers = manager->active_readers;
        waiting_writers = manager->waiting_writers;
        writer_active = manager->writer_active;
        pthread_mutex_unlock(&manager->mutex);
    }

    snprintf(details,
             sizeof(details),
             "mode=%s policy=%s active_readers=%zu waiting_writers=%zu writer_active=%d",
             concurrency_lock_mode_name(mode),
             manager == NULL ? "unknown" : concurrency_lock_policy_name(manager->policy),
             active_readers,
             waiting_writers,
             writer_active);
    runtime_log_event(request_id, event, details);
}

static void close_fd_if_open(int *fd)
{
    if (fd != NULL && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static void signal_stop_handler(int signal_number)
{
    (void)signal_number;

    if (g_signal_runtime != NULL) {
        g_signal_runtime->stop_requested = 1;
        if (g_signal_runtime->listener_fd >= 0) {
            close(g_signal_runtime->listener_fd);
            g_signal_runtime->listener_fd = -1;
        }
    }
}

static int install_signal_handlers(DbServerRuntime *runtime, char *error, size_t error_size)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_stop_handler;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) != 0) {
        snprintf(error, error_size, "failed to install SIGINT handler: %s", strerror(errno));
        return 0;
    }

    if (sigaction(SIGTERM, &action, NULL) != 0) {
        snprintf(error, error_size, "failed to install SIGTERM handler: %s", strerror(errno));
        return 0;
    }

    g_signal_runtime = runtime;
    return 1;
}

static size_t runtime_queue_capacity(int worker_count)
{
    size_t queue_capacity;

    if (worker_count < 1) {
        return SERVER_RUNTIME_MIN_QUEUE_CAPACITY;
    }

    queue_capacity = (size_t)worker_count * SERVER_RUNTIME_QUEUE_CAPACITY_FACTOR;
    if (queue_capacity < SERVER_RUNTIME_MIN_QUEUE_CAPACITY) {
        return SERVER_RUNTIME_MIN_QUEUE_CAPACITY;
    }

    return queue_capacity;
}

static int create_listener_socket(int port, char *error, size_t error_size)
{
    struct sockaddr_in address;
    int listener_fd;
    int reuse_addr = 1;

    listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd < 0) {
        snprintf(error, error_size, "failed to create listen socket: %s", strerror(errno));
        return -1;
    }

    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) != 0) {
        snprintf(error, error_size, "failed to configure SO_REUSEADDR: %s", strerror(errno));
        close(listener_fd);
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);

    if (bind(listener_fd, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        snprintf(error, error_size, "failed to bind listen socket on port %d: %s", port, strerror(errno));
        close(listener_fd);
        return -1;
    }

    if (listen(listener_fd, SERVER_RUNTIME_LISTEN_BACKLOG) != 0) {
        snprintf(error, error_size, "failed to listen on port %d: %s", port, strerror(errno));
        close(listener_fd);
        return -1;
    }

    return listener_fd;
}

static int send_all(int client_fd, const char *buffer, size_t buffer_length)
{
    size_t total_sent = 0;

    while (total_sent < buffer_length) {
        ssize_t sent = send(client_fd,
                            buffer + total_sent,
                            buffer_length - total_sent,
                            SERVER_RUNTIME_SEND_FLAGS);

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }

        if (sent == 0) {
            return 0;
        }

        total_sent += (size_t)sent;
    }

    return 1;
}

static void write_error_and_close_client(int client_fd, int status_code, const char *error_code, const char *message)
{
    char *response_text = NULL;
    char error[256];
    size_t response_length = 0;

    if (http_build_error_response(status_code,
                                  "",
                                  error_code,
                                  message,
                                  &response_text,
                                  &response_length,
                                  error,
                                  sizeof(error))) {
        send_all(client_fd, response_text, response_length);
        http_response_free(response_text);
    }

    close_fd_if_open(&client_fd);
}

static int clamp_affected_rows(long long affected_rows)
{
    if (affected_rows > (long long)INT_MAX) {
        return INT_MAX;
    }

    if (affected_rows < (long long)INT_MIN) {
        return INT_MIN;
    }

    return (int)affected_rows;
}

static int runtime_execute_query(void *user_data,
                                 const char *sql,
                                 http_query_result *result,
                                 char *error,
                                 size_t error_size)
{
    DbServerRuntime *runtime = (DbServerRuntime *)user_data;
    unsigned long long request_id = g_runtime_request_id;
    ConcurrencyLockMode lock_mode;
    server_core_request request;
    server_core_result core_result;
    server_core_status status;

    if (runtime == NULL || result == NULL || error == NULL || error_size == 0U) {
        return 0;
    }

    memset(&request, 0, sizeof(request));
    memset(&core_result, 0, sizeof(core_result));

    request.sql = sql;
    request.schema_path = runtime->config.schema_dir;
    request.data_path = runtime->config.data_dir;

    server_core_result_init(&core_result);
    lock_mode = concurrency_lock_mode_from_sql(sql);
    runtime_log_lock_event(request_id, "lock_wait", &runtime->thread_pool.lock_manager, lock_mode);
    concurrency_lock_manager_acquire(&runtime->thread_pool.lock_manager, lock_mode);
    runtime_log_lock_event(request_id, "lock_acquired", &runtime->thread_pool.lock_manager, lock_mode);
    status = server_core_execute(&runtime->engine, &request, &core_result);
    concurrency_lock_manager_release(&runtime->thread_pool.lock_manager, lock_mode);
    runtime_log_lock_event(request_id, "lock_released", &runtime->thread_pool.lock_manager, lock_mode);

    if (status != SERVER_CORE_STATUS_OK) {
        snprintf(error,
                 error_size,
                 "%s",
                 core_result.message == NULL ? server_core_status_string(status) : core_result.message);
        server_core_result_free(&core_result);
        return 0;
    }

    result->ok = core_result.ok;
    result->affected_rows = clamp_affected_rows(core_result.affected_rows);
    result->message = core_result.message;
    result->output_text = core_result.output_text;

    core_result.message = NULL;
    core_result.output_text = NULL;
    server_core_result_free(&core_result);
    return 1;
}

static void runtime_cleanup_query_result(void *user_data, http_query_result *result)
{
    (void)user_data;

    if (result == NULL) {
        return;
    }

    free((void *)result->message);
    free((void *)result->output_text);
    result->message = NULL;
    result->output_text = NULL;
    result->affected_rows = 0;
    result->ok = 0;
}

static int run_client_job(void *context)
{
    DbServerClientJob *job = (DbServerClientJob *)context;
    int client_fd;
    int success;
    unsigned int active_workers;
    char details[160];

    if (job == NULL || job->runtime == NULL || job->client_fd < 0) {
        return 1;
    }

    client_fd = job->client_fd;
    job->client_fd = -1;
    g_runtime_request_id = job->request_id;
    active_workers = atomic_fetch_add_explicit(&g_runtime_active_workers, 1u, memory_order_relaxed) + 1u;
    snprintf(details,
             sizeof(details),
             "active_workers=%u queue_size=%zu",
             active_workers,
             concurrency_job_queue_size(&job->runtime->thread_pool.queue));
    runtime_log_event(job->request_id, "worker_start", details);

    success = http_server_handle_client(client_fd, &job->runtime->http_config, &job->runtime->http_dependencies);

    active_workers = atomic_fetch_sub_explicit(&g_runtime_active_workers, 1u, memory_order_relaxed) - 1u;
    snprintf(details, sizeof(details), "active_workers=%u ok=%d", active_workers, success ? 1 : 0);
    runtime_log_event(job->request_id, "worker_done", details);
    g_runtime_request_id = 0;
    return success ? 0 : 1;
}

static void cleanup_client_job(void *context)
{
    DbServerClientJob *job = (DbServerClientJob *)context;

    if (job == NULL) {
        return;
    }

    if (job->client_fd >= 0) {
        close_fd_if_open(&job->client_fd);
    }

    free(job);
}

static void shutdown_runtime(DbServerRuntime *runtime, ConcurrencyThreadPoolShutdownMode mode)
{
    if (runtime == NULL) {
        return;
    }

    runtime->stop_requested = 1;
    close_fd_if_open(&runtime->listener_fd);

    if (runtime->thread_pool_started) {
        concurrency_thread_pool_shutdown(&runtime->thread_pool, mode);
        concurrency_thread_pool_destroy(&runtime->thread_pool);
        runtime->thread_pool_started = 0;
    }

    server_core_week8_engine_reset();

    if (g_signal_runtime == runtime) {
        g_signal_runtime = NULL;
    }
}

static void reject_client_with_status(int client_fd, ConcurrencyJobQueueStatus status)
{
    if (status == CONCURRENCY_JOB_QUEUE_FULL) {
        write_error_and_close_client(client_fd, 503, "server_busy", "server queue is full");
        return;
    }

    if (status == CONCURRENCY_JOB_QUEUE_CLOSED) {
        write_error_and_close_client(client_fd, 503, "server_stopping", "server is shutting down");
        return;
    }

    write_error_and_close_client(client_fd, 500, "queue_submit_failed", "failed to schedule the request");
}

static void accept_client(DbServerRuntime *runtime, int client_fd)
{
    DbServerClientJob *job_context;
    ConcurrencyJob job;
    ConcurrencyJobQueueStatus status;
    char details[160];

    job_context = (DbServerClientJob *)calloc(1, sizeof(*job_context));
    if (job_context == NULL) {
        write_error_and_close_client(client_fd, 500, "allocation_failed", "failed to allocate request context");
        return;
    }

    job_context->runtime = runtime;
    job_context->client_fd = client_fd;
    job_context->request_id = atomic_fetch_add_explicit(&g_runtime_next_request_id, 1u, memory_order_relaxed);

    snprintf(details,
             sizeof(details),
             "queue_size=%zu active_workers=%u",
             concurrency_job_queue_size(&runtime->thread_pool.queue),
             atomic_load_explicit(&g_runtime_active_workers, memory_order_relaxed));
    runtime_log_event(job_context->request_id, "request_accepted", details);

    memset(&job, 0, sizeof(job));
    job.lock_mode = CONCURRENCY_LOCK_MODE_NONE;
    job.run = run_client_job;
    job.cleanup = cleanup_client_job;
    job.context = job_context;

    status = concurrency_thread_pool_try_submit(&runtime->thread_pool, &job);
    if (status == CONCURRENCY_JOB_QUEUE_OK) {
        snprintf(details,
                 sizeof(details),
                 "queue_size=%zu active_workers=%u",
                 concurrency_job_queue_size(&runtime->thread_pool.queue),
                 atomic_load_explicit(&g_runtime_active_workers, memory_order_relaxed));
        runtime_log_event(job_context->request_id, "request_queued", details);
        return;
    }

    snprintf(details,
             sizeof(details),
             "queue_status=%d queue_size=%zu active_workers=%u",
             (int)status,
             concurrency_job_queue_size(&runtime->thread_pool.queue),
             atomic_load_explicit(&g_runtime_active_workers, memory_order_relaxed));
    runtime_log_event(job_context->request_id, "request_rejected", details);
    reject_client_with_status(client_fd, status);
    concurrency_job_finish(&job, CONCURRENCY_JOB_RESULT_REJECTED, -1, 0);
}

void db_server_runtime_init(DbServerRuntime *runtime)
{
    if (runtime == NULL) {
        return;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->listener_fd = -1;
}

int db_server_runtime_start(const ServerRuntimeConfig *config, void *server_context, char *error, size_t error_size)
{
    DbServerRuntime *runtime = (DbServerRuntime *)server_context;
    size_t queue_capacity;

    if (config == NULL || runtime == NULL || error == NULL || error_size == 0U) {
        return 0;
    }

    db_server_runtime_init(runtime);
    runtime->config = *config;
    runtime->engine.run = server_core_week8_engine_run;
    runtime->engine.user_data = NULL;
    runtime->http_config.max_request_bytes = HTTP_DEFAULT_MAX_REQUEST_BYTES;
    runtime->http_dependencies.execute_query = runtime_execute_query;
    runtime->http_dependencies.cleanup_query_result = runtime_cleanup_query_result;
    runtime->http_dependencies.user_data = runtime;

    queue_capacity = runtime_queue_capacity(config->worker_count);
    if (!concurrency_thread_pool_init(&runtime->thread_pool,
                                      (size_t)config->worker_count,
                                      queue_capacity,
                                      CONCURRENCY_LOCK_POLICY_SERIAL_ALL)) {
        snprintf(error, error_size, "failed to initialize worker pool");
        return 0;
    }
    runtime->thread_pool_started = 1;
    {
        char details[128];
        snprintf(details,
                 sizeof(details),
                 "workers=%d queue_capacity=%zu lock_policy=%s",
                 config->worker_count,
                 queue_capacity,
                 concurrency_lock_policy_name(CONCURRENCY_LOCK_POLICY_SERIAL_ALL));
        runtime_log_event(0, "runtime_started", details);
    }

    runtime->listener_fd = create_listener_socket(config->port, error, error_size);
    if (runtime->listener_fd < 0) {
        shutdown_runtime(runtime, CONCURRENCY_THREAD_POOL_SHUTDOWN_CANCEL_PENDING);
        return 0;
    }

    if (!install_signal_handlers(runtime, error, error_size)) {
        shutdown_runtime(runtime, CONCURRENCY_THREAD_POOL_SHUTDOWN_CANCEL_PENDING);
        return 0;
    }

    fprintf(stdout, "Listening on 0.0.0.0:%d\n", config->port);
    fflush(stdout);
    return 1;
}

int db_server_runtime_wait(void *server_context, char *error, size_t error_size)
{
    DbServerRuntime *runtime = (DbServerRuntime *)server_context;

    if (runtime == NULL || error == NULL || error_size == 0U) {
        return 0;
    }

    for (;;) {
        int client_fd;

        if (runtime->stop_requested) {
            break;
        }

        client_fd = accept(runtime->listener_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (runtime->stop_requested || errno == EBADF || errno == EINVAL) {
                break;
            }

            snprintf(error, error_size, "accept failed: %s", strerror(errno));
            shutdown_runtime(runtime, CONCURRENCY_THREAD_POOL_SHUTDOWN_CANCEL_PENDING);
            return 0;
        }

        accept_client(runtime, client_fd);
    }

    shutdown_runtime(runtime, CONCURRENCY_THREAD_POOL_SHUTDOWN_DRAIN);
    return 1;
}

void db_server_runtime_request_stop(void *server_context)
{
    DbServerRuntime *runtime = (DbServerRuntime *)server_context;

    if (runtime == NULL) {
        return;
    }

    runtime->stop_requested = 1;
    close_fd_if_open(&runtime->listener_fd);
}
