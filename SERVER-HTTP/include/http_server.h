#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stddef.h>

#include "http_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_query_result {
    int ok;
    int affected_rows;
    const char *message;
    const char *output_text;
} http_query_result;

typedef int (*http_query_executor_fn)(void *user_data,
                                      const char *sql,
                                      http_query_result *result,
                                      char *error,
                                      size_t error_size);

typedef void (*http_query_result_cleanup_fn)(void *user_data,
                                             http_query_result *result);

typedef struct http_server_config {
    size_t max_request_bytes;
} http_server_config;

typedef struct http_server_dependencies {
    http_query_executor_fn execute_query;
    http_query_result_cleanup_fn cleanup_query_result;
    void *user_data;
} http_server_dependencies;

/*
 * Handles exactly one client connection:
 * - reads one HTTP request
 * - routes /health or /query
 * - writes one JSON response
 * - closes the socket
 */
int http_server_handle_client(int client_fd,
                              const http_server_config *config,
                              const http_server_dependencies *dependencies);

#ifdef __cplusplus
}
#endif

#endif
