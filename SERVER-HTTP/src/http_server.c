#include "http_server.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <unistd.h>

#include "thread_pool.h"

#define HTTP_RECEIVE_CHUNK_BYTES 4096u
#define HTTP_DEBUG_SLEEP_MAX_MS 10000u

#ifdef MSG_NOSIGNAL
#define HTTP_SEND_FLAGS MSG_NOSIGNAL
#else
#define HTTP_SEND_FLAGS 0
#endif

static atomic_ullong g_http_request_sequence = 0;

void http_request_trace_init(http_request_trace *trace) {
    if (trace == NULL) {
        return;
    }

    memset(trace, 0, sizeof(*trace));
    trace->worker_index = -1;
}

static int http_text_is_blank(const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;

    if (text == NULL) {
        return 1;
    }

    while (*cursor != '\0') {
        if (!isspace(*cursor)) {
            return 0;
        }
        cursor++;
    }

    return 1;
}

static int http_parse_debug_sleep_ms(const http_request *request,
                                     unsigned int *sleep_ms,
                                     char *error,
                                     size_t error_size) {
    const unsigned char *cursor;
    unsigned int parsed_value = 0;

    if (sleep_ms == NULL || error == NULL || error_size == 0U) {
        return 0;
    }

    *sleep_ms = 0;

    if (request == NULL || !request->debug_sleep_ms_present) {
        return 1;
    }

    if (request->debug_sleep_ms[0] == '\0') {
        snprintf(error,
                 error_size,
                 "X-Debug-Sleep-Ms must be an integer between 0 and %u",
                 HTTP_DEBUG_SLEEP_MAX_MS);
        return 0;
    }

    for (cursor = (const unsigned char *)request->debug_sleep_ms; *cursor != '\0'; cursor++) {
        unsigned int digit;

        if (!isdigit(*cursor)) {
            snprintf(error,
                     error_size,
                     "X-Debug-Sleep-Ms must be an integer between 0 and %u",
                     HTTP_DEBUG_SLEEP_MAX_MS);
            return 0;
        }

        digit = (unsigned int)(*cursor - '0');
        if (parsed_value > (HTTP_DEBUG_SLEEP_MAX_MS - digit) / 10u) {
            snprintf(error,
                     error_size,
                     "X-Debug-Sleep-Ms must be between 0 and %u milliseconds",
                     HTTP_DEBUG_SLEEP_MAX_MS);
            return 0;
        }

        parsed_value = (parsed_value * 10u) + digit;
    }

    *sleep_ms = parsed_value;
    return 1;
}

static void http_sleep_milliseconds(unsigned int sleep_ms) {
    struct timespec requested;
    struct timespec remaining;

    if (sleep_ms == 0) {
        return;
    }

    requested.tv_sec = (time_t)(sleep_ms / 1000u);
    requested.tv_nsec = (long)((sleep_ms % 1000u) * 1000000u);

    while (nanosleep(&requested, &remaining) != 0) {
        if (errno != EINTR) {
            return;
        }

        requested = remaining;
    }
}

static uint64_t http_next_request_id(void) {
    return (uint64_t)atomic_fetch_add_explicit(&g_http_request_sequence, 1u, memory_order_relaxed) + 1u;
}

static void http_trace_set_status(http_request_trace *trace, int status_code) {
    if (trace == NULL) {
        return;
    }

    trace->status_code = status_code;
}

static void http_trace_set_worker_index(http_request_trace *trace, int worker_index) {
    if (trace == NULL || worker_index < 0) {
        return;
    }

    trace->worker_index = worker_index;
}

static void http_trace_copy_request(http_request_trace *trace, const http_request *request) {
    if (trace == NULL || request == NULL) {
        return;
    }

    trace->content_length = request->content_length;
    snprintf(trace->method, sizeof(trace->method), "%s", request->method);
    snprintf(trace->path, sizeof(trace->path), "%s", request->path);
    if (request->debug_sleep_ms_present) {
        snprintf(trace->debug_sleep_ms, sizeof(trace->debug_sleep_ms), "%s", request->debug_sleep_ms);
    } else {
        trace->debug_sleep_ms[0] = '\0';
    }
}

static void http_log_request_event(uint64_t req_id,
                                   const char *event,
                                   const http_request *request,
                                   const http_request_trace *trace,
                                   int status_code) {
    const char *method = "-";
    const char *path = "-";
    const char *debug_sleep_ms = "-";
    int worker_index = -1;
    char status_text[16];
    char worker_text[16];
    size_t content_length = 0;

    if (request != NULL) {
        if (request->method[0] != '\0') {
            method = request->method;
        }
        if (request->path[0] != '\0') {
            path = request->path;
        }
        if (request->debug_sleep_ms_present && request->debug_sleep_ms[0] != '\0') {
            debug_sleep_ms = request->debug_sleep_ms;
        }
        content_length = request->content_length;
    }

    if (trace != NULL) {
        if (trace->worker_index >= 0) {
            worker_index = trace->worker_index;
        }
    } else {
        worker_index = concurrency_thread_pool_current_worker_index();
    }

    if (status_code > 0) {
        snprintf(status_text, sizeof(status_text), "%d", status_code);
    } else {
        snprintf(status_text, sizeof(status_text), "-");
    }

    if (worker_index >= 0) {
        snprintf(worker_text, sizeof(worker_text), "%d", worker_index);
    } else {
        snprintf(worker_text, sizeof(worker_text), "-");
    }

    flockfile(stdout);
    fprintf(stdout,
            "[HTTP] | req_id=%" PRIu64 " | event=%s | thread=%s | method=%s | path=%s | status=%s | bytes=%zu | debug_sleep_ms=%s |\n",
            req_id,
            event,
            worker_text,
            method,
            path,
            status_text,
            content_length,
            debug_sleep_ms);
    funlockfile(stdout);
    fflush(stdout);
}

static void http_close_client(int client_fd) {
    if (client_fd >= 0) {
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
    }
}

static int http_send_all(int client_fd, const char *buffer, size_t buffer_length) {
    size_t bytes_sent = 0;

    while (bytes_sent < buffer_length) {
        ssize_t send_result = send(client_fd,
                                   buffer + bytes_sent,
                                   buffer_length - bytes_sent,
                                   HTTP_SEND_FLAGS);

        if (send_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }

        if (send_result == 0) {
            return 0;
        }

        bytes_sent += (size_t)send_result;
    }

    return 1;
}

static int http_send_json_response(int client_fd, char *response_text, size_t response_length) {
    int success = http_send_all(client_fd, response_text, response_length);
    http_response_free(response_text);
    return success;
}

static int http_write_error_response(int client_fd,
                                     int status_code,
                                     const char *path,
                                     const char *error_code,
                                     const char *message) {
    char build_error[256];
    char *response_text = NULL;
    size_t response_length = 0;

    if (!http_build_error_response(status_code,
                                   path,
                                   error_code,
                                   message,
                                   &response_text,
                                   &response_length,
                                   build_error,
                                   sizeof(build_error))) {
        return 0;
    }

    return http_send_json_response(client_fd, response_text, response_length);
}

static int http_write_health_response(int client_fd) {
    char build_error[256];
    char *response_text = NULL;
    size_t response_length = 0;

    if (!http_build_health_response(&response_text, &response_length, build_error, sizeof(build_error))) {
        return 0;
    }

    return http_send_json_response(client_fd, response_text, response_length);
}

static int http_write_query_response(int client_fd,
                                     int ok,
                                     int status_code,
                                     const char *path,
                                     const char *error_code,
                                     const char *message,
                                     int affected_rows,
                                     const char *output_text) {
    char build_error[256];
    char *response_text = NULL;
    size_t response_length = 0;

    if (!http_build_query_response(ok,
                                   status_code,
                                   path,
                                   error_code,
                                   message,
                                   affected_rows,
                                   output_text,
                                   &response_text,
                                   &response_length,
                                   build_error,
                                   sizeof(build_error))) {
        return 0;
    }

    return http_send_json_response(client_fd, response_text, response_length);
}

static int http_return_error_response(int client_fd,
                                      int status_code,
                                      const char *path,
                                      const char *error_code,
                                      const char *message,
                                      int *status_out) {
    if (status_out != NULL) {
        *status_out = status_code;
    }

    return http_write_error_response(client_fd, status_code, path, error_code, message);
}

static int http_return_health_response(int client_fd, int *status_out) {
    if (status_out != NULL) {
        *status_out = 200;
    }

    return http_write_health_response(client_fd);
}

static int http_return_query_response(int client_fd,
                                      int ok,
                                      int status_code,
                                      const char *path,
                                      const char *error_code,
                                      const char *message,
                                      int affected_rows,
                                      const char *output_text,
                                      int *status_out) {
    if (status_out != NULL) {
        *status_out = status_code;
    }

    return http_write_query_response(client_fd,
                                     ok,
                                     status_code,
                                     path,
                                     error_code,
                                     message,
                                     affected_rows,
                                     output_text);
}

static int http_grow_buffer(char **buffer,
                            size_t *capacity,
                            size_t minimum_capacity,
                            size_t maximum_capacity) {
    char *new_buffer;
    size_t new_capacity = *capacity == 0 ? HTTP_RECEIVE_CHUNK_BYTES : *capacity;

    while (new_capacity < minimum_capacity) {
        if (new_capacity >= maximum_capacity) {
            new_capacity = maximum_capacity;
            break;
        }

        if (new_capacity > maximum_capacity / 2u) {
            new_capacity = maximum_capacity;
        } else {
            new_capacity *= 2u;
        }
    }

    if (new_capacity < minimum_capacity) {
        return 0;
    }

    new_buffer = (char *)realloc(*buffer, new_capacity);
    if (new_buffer == NULL) {
        return 0;
    }

    *buffer = new_buffer;
    *capacity = new_capacity;
    return 1;
}

static int http_route_request(int client_fd,
                              const http_request *request,
                              const http_server_dependencies *dependencies,
                              int *status_out) {
    unsigned int debug_sleep_ms = 0;
    http_query_result query_result;
    char execution_error[256];

    memset(&query_result, 0, sizeof(query_result));

    if (strcmp(request->path, "/health") == 0) {
        if (strcmp(request->method, "GET") != 0) {
            return http_return_error_response(client_fd,
                                              405,
                                              request->path,
                                              "method_not_allowed",
                                              "GET is required for /health",
                                              status_out);
        }

        if (request->content_length != 0) {
            return http_return_error_response(client_fd,
                                              400,
                                              request->path,
                                              "invalid_body",
                                              "/health does not accept a request body",
                                              status_out);
        }

        return http_return_health_response(client_fd, status_out);
    }

    if (strcmp(request->path, "/query") == 0) {
        if (strcmp(request->method, "POST") != 0) {
            return http_return_error_response(client_fd,
                                              405,
                                              request->path,
                                              "method_not_allowed",
                                              "POST is required for /query",
                                              status_out);
        }

        if (!http_content_type_is_sql(request->content_type)) {
            return http_return_error_response(client_fd,
                                              415,
                                              request->path,
                                              "unsupported_media_type",
                                              "/query expects a raw SQL body",
                                              status_out);
        }

        if (request->content_length == 0 || http_text_is_blank(request->body)) {
            return http_return_error_response(client_fd,
                                              400,
                                              request->path,
                                              "invalid_body",
                                              "request body must contain a SQL statement",
                                              status_out);
        }

        if (dependencies == NULL || dependencies->execute_query == NULL) {
            return http_return_error_response(client_fd,
                                              500,
                                              request->path,
                                              "missing_dependency",
                                              "query executor callback is not configured",
                                              status_out);
        }

        if (!http_parse_debug_sleep_ms(request,
                                       &debug_sleep_ms,
                                       execution_error,
                                       sizeof(execution_error))) {
            return http_return_error_response(client_fd,
                                              400,
                                              request->path,
                                              "invalid_header",
                                              execution_error,
                                              status_out);
        }

        http_sleep_milliseconds(debug_sleep_ms);

        if (!dependencies->execute_query(dependencies->user_data,
                                         request->body,
                                         &query_result,
                                         execution_error,
                                         sizeof(execution_error))) {
            int sent;

            sent = http_return_error_response(client_fd,
                                              500,
                                              request->path,
                                              "query_execution_failed",
                                              execution_error[0] == '\0'
                                                  ? "query execution callback failed"
                                                  : execution_error,
                                              status_out);
            if (dependencies->cleanup_query_result != NULL) {
                dependencies->cleanup_query_result(dependencies->user_data, &query_result);
            }
            return sent;
        }

        if (!query_result.ok) {
            int sent = http_return_query_response(client_fd,
                                                  0,
                                                  400,
                                                  request->path,
                                                  "query_failed",
                                                  query_result.message == NULL ? "query execution failed" : query_result.message,
                                                  query_result.affected_rows,
                                                  query_result.output_text,
                                                  status_out);
            if (dependencies->cleanup_query_result != NULL) {
                dependencies->cleanup_query_result(dependencies->user_data, &query_result);
            }
            return sent;
        }

        {
            int sent = http_return_query_response(client_fd,
                                                  1,
                                                  200,
                                                  request->path,
                                                  NULL,
                                                  query_result.message == NULL ? "query completed" : query_result.message,
                                                  query_result.affected_rows,
                                                  query_result.output_text,
                                                  status_out);
            if (dependencies->cleanup_query_result != NULL) {
                dependencies->cleanup_query_result(dependencies->user_data, &query_result);
            }
            return sent;
        }
    }

    return http_return_error_response(client_fd,
                                      404,
                                      request->path,
                                      "path_not_found",
                                      "unsupported endpoint",
                                      status_out);
}

int http_server_handle_client(int client_fd,
                              const http_server_config *config,
                              const http_server_dependencies *dependencies,
                              http_request_trace *trace) {
    char parse_error[256];
    char *request_buffer = NULL;
    size_t buffer_capacity = 0;
    size_t buffer_length = 0;
    size_t request_length = 0;
    size_t max_request_bytes = HTTP_DEFAULT_MAX_REQUEST_BYTES;
    int success = 0;
    int response_status_code = 0;
    http_request request;
    uint64_t req_id = 0;

    memset(&request, 0, sizeof(request));

    if (trace != NULL) {
        if (trace->worker_index < 0) {
            http_trace_set_worker_index(trace, concurrency_thread_pool_current_worker_index());
        }

        if (trace->req_id == 0) {
            trace->req_id = http_next_request_id();
        }
    }

    if (config != NULL && config->max_request_bytes > 0) {
        max_request_bytes = config->max_request_bytes;
    }

    while (1) {
        http_probe_result probe_result;
        char probe_error[256];
        ssize_t bytes_read;

        if (!http_grow_buffer(&request_buffer,
                              &buffer_capacity,
                              buffer_length + 1u,
                              max_request_bytes)) {
            if (buffer_length >= max_request_bytes) {
                http_trace_set_status(trace, 413);
                http_write_error_response(client_fd,
                                          413,
                                          "",
                                          "payload_too_large",
                                          "request exceeds the configured size limit");
            } else {
                http_trace_set_status(trace, 500);
                http_write_error_response(client_fd,
                                          500,
                                          "",
                                          "allocation_failed",
                                          "failed to allocate the request buffer");
            }
            goto cleanup;
        }

        bytes_read = recv(client_fd,
                          request_buffer + buffer_length,
                          buffer_capacity - buffer_length,
                          0);

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }

            http_trace_set_status(trace, 500);
            http_write_error_response(client_fd,
                                      500,
                                      "",
                                      "read_failed",
                                      "failed to read the client request");
            goto cleanup;
        }

        if (bytes_read == 0) {
            if (buffer_length == 0) {
                success = 1;
            } else {
                http_trace_set_status(trace, 400);
                http_write_error_response(client_fd,
                                          400,
                                          "",
                                          "incomplete_request",
                                          "client closed the connection before the request completed");
            }
            goto cleanup;
        }

        buffer_length += (size_t)bytes_read;

        probe_result = http_request_probe(request_buffer,
                                          buffer_length,
                                          max_request_bytes,
                                          &request_length,
                                          probe_error,
                                          sizeof(probe_error));

        if (request_length > max_request_bytes) {
            http_trace_set_status(trace, 413);
            http_write_error_response(client_fd,
                                      413,
                                      "",
                                      "payload_too_large",
                                      "request exceeds the configured size limit");
            goto cleanup;
        }

        if (probe_result == HTTP_PROBE_INVALID) {
            http_trace_set_status(trace, 400);
            http_write_error_response(client_fd,
                                      400,
                                      "",
                                      "invalid_request",
                                      probe_error);
            goto cleanup;
        }

        if (probe_result == HTTP_PROBE_COMPLETE) {
            break;
        }
    }

    if (!http_parse_request(request_buffer,
                            request_length,
                            &request,
                            parse_error,
                            sizeof(parse_error))) {
        http_trace_set_status(trace, 400);
        http_write_error_response(client_fd,
                                  400,
                                  "",
                                  "invalid_request",
                                  parse_error);
        goto cleanup;
    }

    if (trace != NULL) {
        http_trace_copy_request(trace, &request);
        req_id = trace->req_id;
    } else {
        req_id = http_next_request_id();
    }
    http_log_request_event(req_id, "요청 수신", &request, trace, 0);

    success = http_route_request(client_fd, &request, dependencies, &response_status_code);
    http_trace_set_status(trace, response_status_code);
    http_log_request_event(req_id, "응답 완료", &request, trace, response_status_code);

cleanup:
    http_request_free(&request);
    free(request_buffer);
    http_close_client(client_fd);
    return success;
}
