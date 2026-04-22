#ifndef HTTP_PROTOCOL_H
#define HTTP_PROTOCOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_DEFAULT_MAX_REQUEST_BYTES 65536u

typedef enum http_probe_result {
    HTTP_PROBE_INVALID = -1,
    HTTP_PROBE_INCOMPLETE = 0,
    HTTP_PROBE_COMPLETE = 1
} http_probe_result;

typedef struct http_request {
    char method[16];
    char path[128];
    char version[16];
    char content_type[64];
    char debug_sleep_ms[16];
    int debug_sleep_ms_present;
    size_t content_length;
    char *body;
} http_request;

/*
 * Inspects the current receive buffer and reports whether a full HTTP request
 * has arrived. Only fixed-size bodies via Content-Length are supported.
 */
http_probe_result http_request_probe(const char *buffer,
                                     size_t buffer_length,
                                     size_t max_request_bytes,
                                     size_t *request_length,
                                     char *error,
                                     size_t error_size);

/*
 * Parses a complete HTTP request into a null-terminated request structure.
 */
int http_parse_request(const char *buffer,
                       size_t buffer_length,
                       http_request *request,
                       char *error,
                       size_t error_size);

void http_request_free(http_request *request);

/*
 * Returns 1 when the Content-Type is acceptable for raw SQL bodies.
 * Empty values are also accepted to keep the MVP client contract simple.
 */
int http_content_type_is_sql(const char *content_type);

int http_build_health_response(char **response_text,
                               size_t *response_length,
                               char *error,
                               size_t error_size);

int http_build_query_response(int ok,
                              int status_code,
                              const char *path,
                              const char *error_code,
                              const char *message,
                              int affected_rows,
                              const char *output_text,
                              char **response_text,
                              size_t *response_length,
                              char *error,
                              size_t error_size);

int http_build_error_response(int status_code,
                              const char *path,
                              const char *error_code,
                              const char *message,
                              char **response_text,
                              size_t *response_length,
                              char *error,
                              size_t error_size);

void http_response_free(char *response_text);

#ifdef __cplusplus
}
#endif

#endif
