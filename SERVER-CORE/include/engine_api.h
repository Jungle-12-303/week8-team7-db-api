#ifndef SERVER_CORE_ENGINE_API_H
#define SERVER_CORE_ENGINE_API_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SERVER_CORE_AFFECTED_ROWS_UNKNOWN (-1LL)
#define SERVER_CORE_MESSAGE_CAPACITY 512U

typedef enum server_core_status {
    SERVER_CORE_STATUS_OK = 0,
    SERVER_CORE_STATUS_INVALID_ARGUMENT = 1,
    SERVER_CORE_STATUS_NO_MEMORY = 2,
    SERVER_CORE_STATUS_CAPTURE_ERROR = 3,
    SERVER_CORE_STATUS_ENGINE_NOT_CONFIGURED = 4
} server_core_status;

typedef struct server_core_request {
    const char *sql;
    const char *schema_path;
    const char *data_path;
} server_core_request;

typedef struct server_core_result {
    int ok;
    long long affected_rows;
    char *message;
    char *output_text;
} server_core_result;

/*
 * Engine contract:
 * - `out` is owned by the adapter and must not be closed by the engine.
 * - Any text written to `out` is captured into `result->output_text`.
 * - `message_buffer` should contain a short summary for API responses.
 * - Return non-zero for a logical SQL success, zero for SQL/engine failure.
 * - Set `*affected_rows` when known, otherwise leave it unchanged.
 */
typedef int (*server_core_engine_run_fn)(
    void *user_data,
    const server_core_request *request,
    FILE *out,
    long long *affected_rows,
    char *message_buffer,
    size_t message_buffer_size);

typedef struct server_core_engine {
    server_core_engine_run_fn run;
    void *user_data;
} server_core_engine;

void server_core_result_init(server_core_result *result);
void server_core_result_reset(server_core_result *result);
void server_core_result_free(server_core_result *result);

const char *server_core_status_string(server_core_status status);

/*
 * Adapter behavior:
 * - `result` must be zero-initialized or passed through
 *   `server_core_result_init` before the first call.
 * - `result->message` and `result->output_text` are always heap-owned strings
 *   after a successful adapter call and must be released with
 *   `server_core_result_free`.
 * - SQL failures are reported through `result->ok == 0` while the function
 *   still returns `SERVER_CORE_STATUS_OK`.
 * - Adapter failures such as bad arguments, memory errors, or output capture
 *   failures are returned as `server_core_status` values.
 */
server_core_status server_core_execute(
    const server_core_engine *engine,
    const server_core_request *request,
    server_core_result *result);

#ifdef __cplusplus
}
#endif

#endif
