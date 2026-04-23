#include "server_runtime/db_server_runtime.h"

#include "http_protocol.h"
#include "week8_engine.h"

#include <errno.h>
#include <inttypes.h>
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
    http_request_trace trace;
} DbServerClientJob;

static DbServerRuntime *g_signal_runtime = NULL;
static atomic_ullong g_runtime_request_sequence = 0;

static uint64_t runtime_next_request_id(void)
{
    return (uint64_t)atomic_fetch_add_explicit(&g_runtime_request_sequence, 1u, memory_order_relaxed) + 1u;
}

static void runtime_log_event(const http_request_trace *trace,
                              const char *event,
                              int worker_index,
                              const char *result,
                              int run_status)
{
    const char *method = "-";
    const char *path = "-";
    const char *status_text = "-";
    const char *result_text = "-";
    char status_buffer[16];
    uint64_t req_id = 0;

    if (trace != NULL) {
        req_id = trace->req_id;
        if (trace->method[0] != '\0') {
            method = trace->method;
        }
        if (trace->path[0] != '\0') {
            path = trace->path;
        }
        if (trace->status_code > 0) {
            snprintf(status_buffer, sizeof(status_buffer), "%d", trace->status_code);
            status_text = status_buffer;
        }
    }

    if (result != NULL && result[0] != '\0') {
        result_text = result;
    }

    flockfile(stdout);
    fprintf(stdout,
            "[RUNTIME] | req_id=%" PRIu64 " | event=%s | thread=%d | method=%s | path=%s | status=%s | result=%s | run_status=%d |\n",
            req_id,
            event,
            worker_index,
            method,
            path,
            status_text,
            result_text,
            run_status);
    funlockfile(stdout);
    fflush(stdout);
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
    concurrency_lock_manager_acquire(&runtime->thread_pool.lock_manager, lock_mode);
    status = server_core_execute(&runtime->engine, &request, &core_result);
    concurrency_lock_manager_release(&runtime->thread_pool.lock_manager, lock_mode);

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

    if (job == NULL || job->runtime == NULL || job->client_fd < 0) {
        return 1;
    }

    client_fd = job->client_fd;
    job->client_fd = -1;
    success = http_server_handle_client(
        client_fd,
        &job->runtime->http_config,
        &job->runtime->http_dependencies,
        &job->trace
    );
    return success ? 0 : 1;
}

static void assign_client_job(void *context, int worker_index)
{
    DbServerClientJob *job = (DbServerClientJob *)context;

    if (job == NULL) {
        return;
    }

    job->trace.worker_index = worker_index;
    runtime_log_event(&job->trace, "스레드 할당", worker_index, NULL, 0);
}

static void notify_client_job(void *context,
                              ConcurrencyJobResult result,
                              int worker_index,
                              int run_status)
{
    DbServerClientJob *job = (DbServerClientJob *)context;

    if (job == NULL) {
        return;
    }

    runtime_log_event(
        &job->trace,
        "작업 종료",
        worker_index,
        concurrency_job_result_name(result),
        run_status
    );
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

    job_context = (DbServerClientJob *)calloc(1, sizeof(*job_context));
    if (job_context == NULL) {
        write_error_and_close_client(client_fd, 500, "allocation_failed", "failed to allocate request context");
        return;
    }

    job_context->runtime = runtime;
    job_context->client_fd = client_fd;
    http_request_trace_init(&job_context->trace);
    job_context->trace.req_id = runtime_next_request_id();

    memset(&job, 0, sizeof(job));
    job.lock_mode = CONCURRENCY_LOCK_MODE_NONE;
    job.assigned = assign_client_job;
    job.run = run_client_job;
    job.notify = notify_client_job;
    job.cleanup = cleanup_client_job;
    job.context = job_context;

    status = concurrency_thread_pool_try_submit(&runtime->thread_pool, &job);
    if (status == CONCURRENCY_JOB_QUEUE_OK) {
        return;
    }

    reject_client_with_status(client_fd, status);
    job_context->client_fd = -1;
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
