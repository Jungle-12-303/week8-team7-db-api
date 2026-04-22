#include "http_server.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <unistd.h>

#define HTTP_RECEIVE_CHUNK_BYTES 4096u

#ifdef MSG_NOSIGNAL
#define HTTP_SEND_FLAGS MSG_NOSIGNAL
#else
#define HTTP_SEND_FLAGS 0
#endif

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
                              const http_server_dependencies *dependencies) {
    http_query_result query_result;
    char execution_error[256];

    memset(&query_result, 0, sizeof(query_result));

    if (strcmp(request->path, "/health") == 0) {
        if (strcmp(request->method, "GET") != 0) {
            return http_write_error_response(client_fd,
                                             405,
                                             request->path,
                                             "method_not_allowed",
                                             "GET is required for /health");
        }

        if (request->content_length != 0) {
            return http_write_error_response(client_fd,
                                             400,
                                             request->path,
                                             "invalid_body",
                                             "/health does not accept a request body");
        }

        return http_write_health_response(client_fd);
    }

    if (strcmp(request->path, "/query") == 0) {
        if (strcmp(request->method, "POST") != 0) {
            return http_write_error_response(client_fd,
                                             405,
                                             request->path,
                                             "method_not_allowed",
                                             "POST is required for /query");
        }

        if (!http_content_type_is_sql(request->content_type)) {
            return http_write_error_response(client_fd,
                                             415,
                                             request->path,
                                             "unsupported_media_type",
                                             "/query expects a raw SQL body");
        }

        if (request->content_length == 0 || http_text_is_blank(request->body)) {
            return http_write_error_response(client_fd,
                                             400,
                                             request->path,
                                             "invalid_body",
                                             "request body must contain a SQL statement");
        }

        if (dependencies == NULL || dependencies->execute_query == NULL) {
            return http_write_error_response(client_fd,
                                             500,
                                             request->path,
                                             "missing_dependency",
                                             "query executor callback is not configured");
        }

        if (!dependencies->execute_query(dependencies->user_data,
                                         request->body,
                                         &query_result,
                                         execution_error,
                                         sizeof(execution_error))) {
            int sent;

            sent = http_write_error_response(client_fd,
                                             500,
                                             request->path,
                                             "query_execution_failed",
                                             execution_error[0] == '\0'
                                                 ? "query execution callback failed"
                                                 : execution_error);
            if (dependencies->cleanup_query_result != NULL) {
                dependencies->cleanup_query_result(dependencies->user_data, &query_result);
            }
            return sent;
        }

        if (!query_result.ok) {
            int sent = http_write_query_response(client_fd,
                                                 0,
                                                 400,
                                                 request->path,
                                                 "query_failed",
                                                 query_result.message == NULL ? "query execution failed" : query_result.message,
                                                 query_result.affected_rows,
                                                 query_result.output_text);
            if (dependencies->cleanup_query_result != NULL) {
                dependencies->cleanup_query_result(dependencies->user_data, &query_result);
            }
            return sent;
        }

        {
            int sent = http_write_query_response(client_fd,
                                                 1,
                                                 200,
                                                 request->path,
                                                 NULL,
                                                 query_result.message == NULL ? "query completed" : query_result.message,
                                                 query_result.affected_rows,
                                                 query_result.output_text);
            if (dependencies->cleanup_query_result != NULL) {
                dependencies->cleanup_query_result(dependencies->user_data, &query_result);
            }
            return sent;
        }
    }

    return http_write_error_response(client_fd,
                                     404,
                                     request->path,
                                     "path_not_found",
                                     "unsupported endpoint");
}

int http_server_handle_client(int client_fd,
                              const http_server_config *config,
                              const http_server_dependencies *dependencies) {
    char parse_error[256];
    char *request_buffer = NULL;
    size_t buffer_capacity = 0;
    size_t buffer_length = 0;
    size_t request_length = 0;
    size_t max_request_bytes = HTTP_DEFAULT_MAX_REQUEST_BYTES;
    int success = 0;
    http_request request;

    memset(&request, 0, sizeof(request));

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
                http_write_error_response(client_fd,
                                          413,
                                          "",
                                          "payload_too_large",
                                          "request exceeds the configured size limit");
            } else {
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
            http_write_error_response(client_fd,
                                      413,
                                      "",
                                      "payload_too_large",
                                      "request exceeds the configured size limit");
            goto cleanup;
        }

        if (probe_result == HTTP_PROBE_INVALID) {
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
        http_write_error_response(client_fd,
                                  400,
                                  "",
                                  "invalid_request",
                                  parse_error);
        goto cleanup;
    }

    success = http_route_request(client_fd, &request, dependencies);

cleanup:
    http_request_free(&request);
    free(request_buffer);
    http_close_client(client_fd);
    return success;
}
