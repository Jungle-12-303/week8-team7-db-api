#include "http_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct http_head_metadata {
    size_t header_length;
    size_t content_length;
} http_head_metadata;

static const char *http_find_header_end(const char *buffer, size_t buffer_length, size_t *header_length) {
    size_t index;

    if (buffer == NULL) {
        return NULL;
    }

    for (index = 0; index + 3 < buffer_length; index++) {
        if (buffer[index] == '\r' &&
            buffer[index + 1] == '\n' &&
            buffer[index + 2] == '\r' &&
            buffer[index + 3] == '\n') {
            if (header_length != NULL) {
                *header_length = index + 4;
            }
            return buffer + index;
        }
    }

    return NULL;
}

static const char *http_find_crlf(const char *start, const char *end) {
    const char *cursor;

    if (start == NULL || end == NULL || start > end) {
        return NULL;
    }

    for (cursor = start; cursor + 1 < end; cursor++) {
        if (cursor[0] == '\r' && cursor[1] == '\n') {
            return cursor;
        }
    }

    return NULL;
}

static int http_ascii_equals_ignore_case(const char *left, size_t left_length, const char *right) {
    size_t index;
    size_t right_length;

    if (left == NULL || right == NULL) {
        return 0;
    }

    right_length = strlen(right);
    if (left_length != right_length) {
        return 0;
    }

    for (index = 0; index < left_length; index++) {
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index])) {
            return 0;
        }
    }

    return 1;
}

static void http_trim_span(const char **start, const char **end) {
    while (*start < *end && isspace((unsigned char)**start)) {
        (*start)++;
    }

    while (*start < *end && isspace((unsigned char)*((*end) - 1))) {
        (*end)--;
    }
}

static int http_copy_span(char *destination,
                          size_t destination_size,
                          const char *start,
                          const char *end,
                          const char *field_name,
                          char *error,
                          size_t error_size) {
    size_t span_length;

    if (destination == NULL || destination_size == 0 || start == NULL || end == NULL || start > end) {
        snprintf(error, error_size, "invalid %s span", field_name);
        return 0;
    }

    span_length = (size_t)(end - start);
    if (span_length == 0) {
        snprintf(error, error_size, "missing %s", field_name);
        return 0;
    }

    if (span_length >= destination_size) {
        snprintf(error, error_size, "%s is too long", field_name);
        return 0;
    }

    memcpy(destination, start, span_length);
    destination[span_length] = '\0';
    return 1;
}

static int http_parse_request_line(const char *buffer,
                                   const char *line_end,
                                   http_request *request,
                                   char *error,
                                   size_t error_size) {
    const char *first_space;
    const char *second_space;
    const char *method_start = buffer;
    const char *path_start;
    const char *version_start;

    if (buffer == NULL || line_end == NULL || request == NULL) {
        snprintf(error, error_size, "malformed request line");
        return 0;
    }

    first_space = memchr(buffer, ' ', (size_t)(line_end - buffer));
    if (first_space == NULL) {
        snprintf(error, error_size, "request line is missing the path token");
        return 0;
    }

    second_space = memchr(first_space + 1, ' ', (size_t)(line_end - (first_space + 1)));
    if (second_space == NULL) {
        snprintf(error, error_size, "request line is missing the HTTP version");
        return 0;
    }

    if (memchr(second_space + 1, ' ', (size_t)(line_end - (second_space + 1))) != NULL) {
        snprintf(error, error_size, "request line has too many tokens");
        return 0;
    }

    path_start = first_space + 1;
    version_start = second_space + 1;

    if (!http_copy_span(request->method,
                        sizeof(request->method),
                        method_start,
                        first_space,
                        "HTTP method",
                        error,
                        error_size)) {
        return 0;
    }

    if (!http_copy_span(request->path,
                        sizeof(request->path),
                        path_start,
                        second_space,
                        "request path",
                        error,
                        error_size)) {
        return 0;
    }

    if (!http_copy_span(request->version,
                        sizeof(request->version),
                        version_start,
                        line_end,
                        "HTTP version",
                        error,
                        error_size)) {
        return 0;
    }

    if (strcmp(request->version, "HTTP/1.1") != 0 && strcmp(request->version, "HTTP/1.0") != 0) {
        snprintf(error, error_size, "unsupported HTTP version: %s", request->version);
        return 0;
    }

    if (request->path[0] != '/') {
        snprintf(error, error_size, "request path must start with '/'");
        return 0;
    }

    return 1;
}

static int http_parse_content_length(const char *value_start,
                                     const char *value_end,
                                     size_t *content_length,
                                     char *error,
                                     size_t error_size) {
    const char *cursor;
    size_t parsed_value = 0;

    http_trim_span(&value_start, &value_end);
    if (value_start == value_end) {
        snprintf(error, error_size, "Content-Length header is empty");
        return 0;
    }

    for (cursor = value_start; cursor < value_end; cursor++) {
        unsigned char character = (unsigned char)*cursor;

        if (!isdigit(character)) {
            snprintf(error, error_size, "Content-Length must be a decimal number");
            return 0;
        }

        if (parsed_value > (SIZE_MAX - (size_t)(character - '0')) / 10u) {
            snprintf(error, error_size, "Content-Length is too large");
            return 0;
        }

        parsed_value = (parsed_value * 10u) + (size_t)(character - '0');
    }

    *content_length = parsed_value;
    return 1;
}

static int http_parse_headers(const char *headers_start,
                              const char *headers_end,
                              http_request *request,
                              char *error,
                              size_t error_size) {
    const char *cursor = headers_start;
    int saw_content_length = 0;
    int saw_content_type = 0;

    while (cursor < headers_end) {
        const char *line_end = http_find_crlf(cursor, headers_end);
        const char *colon;
        const char *name_start = cursor;
        const char *name_end;
        const char *value_start;
        const char *value_end;

        if (line_end == NULL) {
            snprintf(error, error_size, "header line is not terminated with CRLF");
            return 0;
        }

        if (line_end == cursor) {
            cursor = line_end + 2;
            continue;
        }

        colon = memchr(cursor, ':', (size_t)(line_end - cursor));
        if (colon == NULL) {
            snprintf(error, error_size, "header is missing ':' separator");
            return 0;
        }

        name_end = colon;
        value_start = colon + 1;
        value_end = line_end;
        http_trim_span(&name_start, &name_end);
        http_trim_span(&value_start, &value_end);

        if (name_start == name_end) {
            snprintf(error, error_size, "header name is empty");
            return 0;
        }

        if (http_ascii_equals_ignore_case(name_start, (size_t)(name_end - name_start), "Content-Length")) {
            if (saw_content_length) {
                snprintf(error, error_size, "duplicate Content-Length header");
                return 0;
            }

            if (!http_parse_content_length(value_start, value_end, &request->content_length, error, error_size)) {
                return 0;
            }

            saw_content_length = 1;
        } else if (http_ascii_equals_ignore_case(name_start, (size_t)(name_end - name_start), "Content-Type")) {
            if (saw_content_type) {
                snprintf(error, error_size, "duplicate Content-Type header");
                return 0;
            }

            if (value_start != value_end &&
                !http_copy_span(request->content_type,
                                sizeof(request->content_type),
                                value_start,
                                value_end,
                                "Content-Type",
                                error,
                                error_size)) {
                return 0;
            }

            saw_content_type = 1;
        } else if (http_ascii_equals_ignore_case(name_start, (size_t)(name_end - name_start), "Transfer-Encoding")) {
            snprintf(error, error_size, "Transfer-Encoding is not supported");
            return 0;
        }

        cursor = line_end + 2;
    }

    return 1;
}

static int http_extract_head_metadata(const char *buffer,
                                      size_t buffer_length,
                                      http_head_metadata *metadata,
                                      char *error,
                                      size_t error_size) {
    http_request request;
    const char *request_line_end;
    size_t header_length = 0;

    memset(&request, 0, sizeof(request));

    if (http_find_header_end(buffer, buffer_length, &header_length) == NULL) {
        snprintf(error, error_size, "request headers are incomplete");
        return 0;
    }

    request_line_end = http_find_crlf(buffer, buffer + header_length);
    if (request_line_end == NULL) {
        snprintf(error, error_size, "request line is missing CRLF");
        return 0;
    }

    if (!http_parse_request_line(buffer, request_line_end, &request, error, error_size)) {
        return 0;
    }

    if (!http_parse_headers(request_line_end + 2, buffer + header_length - 2, &request, error, error_size)) {
        return 0;
    }

    metadata->header_length = header_length;
    metadata->content_length = request.content_length;
    return 1;
}

static int http_json_escape(const char *input,
                            char **escaped_output,
                            char *error,
                            size_t error_size) {
    const unsigned char *cursor;
    char *escaped;
    char *write_cursor;
    size_t escaped_length = 0;

    if (input == NULL) {
        input = "";
    }

    for (cursor = (const unsigned char *)input; *cursor != '\0'; cursor++) {
        switch (*cursor) {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                escaped_length += 2;
                break;
            default:
                if (*cursor < 0x20u) {
                    escaped_length += 6;
                } else {
                    escaped_length += 1;
                }
                break;
        }
    }

    escaped = (char *)malloc(escaped_length + 1);
    if (escaped == NULL) {
        snprintf(error, error_size, "out of memory while escaping JSON string");
        return 0;
    }

    write_cursor = escaped;
    for (cursor = (const unsigned char *)input; *cursor != '\0'; cursor++) {
        switch (*cursor) {
            case '\"':
                *write_cursor++ = '\\';
                *write_cursor++ = '\"';
                break;
            case '\\':
                *write_cursor++ = '\\';
                *write_cursor++ = '\\';
                break;
            case '\b':
                *write_cursor++ = '\\';
                *write_cursor++ = 'b';
                break;
            case '\f':
                *write_cursor++ = '\\';
                *write_cursor++ = 'f';
                break;
            case '\n':
                *write_cursor++ = '\\';
                *write_cursor++ = 'n';
                break;
            case '\r':
                *write_cursor++ = '\\';
                *write_cursor++ = 'r';
                break;
            case '\t':
                *write_cursor++ = '\\';
                *write_cursor++ = 't';
                break;
            default:
                if (*cursor < 0x20u) {
                    snprintf(write_cursor, 7, "\\u%04x", (unsigned int)*cursor);
                    write_cursor += 6;
                } else {
                    *write_cursor++ = (char)*cursor;
                }
                break;
        }
    }

    *write_cursor = '\0';
    *escaped_output = escaped;
    return 1;
}

static const char *http_reason_phrase(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 413:
            return "Payload Too Large";
        case 415:
            return "Unsupported Media Type";
        case 500:
            return "Internal Server Error";
        default:
            return "Internal Server Error";
    }
}

static int http_wrap_json_response(int status_code,
                                   const char *json_body,
                                   char **response_text,
                                   size_t *response_length,
                                   char *error,
                                   size_t error_size) {
    const char *reason = http_reason_phrase(status_code);
    int response_bytes;
    char *response;

    response_bytes = snprintf(NULL,
                              0,
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n"
                              "%s",
                              status_code,
                              reason,
                              strlen(json_body),
                              json_body);
    if (response_bytes < 0) {
        snprintf(error, error_size, "failed to calculate HTTP response size");
        return 0;
    }

    response = (char *)malloc((size_t)response_bytes + 1);
    if (response == NULL) {
        snprintf(error, error_size, "out of memory while building HTTP response");
        return 0;
    }

    snprintf(response,
             (size_t)response_bytes + 1,
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status_code,
             reason,
             strlen(json_body),
             json_body);

    *response_text = response;
    *response_length = (size_t)response_bytes;
    return 1;
}

http_probe_result http_request_probe(const char *buffer,
                                     size_t buffer_length,
                                     size_t max_request_bytes,
                                     size_t *request_length,
                                     char *error,
                                     size_t error_size) {
    http_head_metadata metadata;
    size_t effective_max = max_request_bytes == 0 ? HTTP_DEFAULT_MAX_REQUEST_BYTES : max_request_bytes;

    if (request_length != NULL) {
        *request_length = 0;
    }

    if (buffer == NULL) {
        snprintf(error, error_size, "request buffer is null");
        return HTTP_PROBE_INVALID;
    }

    if (buffer_length > effective_max) {
        snprintf(error, error_size, "request exceeds configured size limit");
        return HTTP_PROBE_INVALID;
    }

    if (http_find_header_end(buffer, buffer_length, NULL) == NULL) {
        return HTTP_PROBE_INCOMPLETE;
    }

    if (!http_extract_head_metadata(buffer, buffer_length, &metadata, error, error_size)) {
        return HTTP_PROBE_INVALID;
    }

    if (metadata.header_length > SIZE_MAX - metadata.content_length) {
        snprintf(error, error_size, "request size overflows the platform limit");
        return HTTP_PROBE_INVALID;
    }

    if (request_length != NULL) {
        *request_length = metadata.header_length + metadata.content_length;
    }

    if (metadata.header_length + metadata.content_length > effective_max) {
        return HTTP_PROBE_INCOMPLETE;
    }

    if (buffer_length >= metadata.header_length + metadata.content_length) {
        return HTTP_PROBE_COMPLETE;
    }

    return HTTP_PROBE_INCOMPLETE;
}

int http_parse_request(const char *buffer,
                       size_t buffer_length,
                       http_request *request,
                       char *error,
                       size_t error_size) {
    const char *request_line_end;
    size_t header_length = 0;
    size_t body_length;

    if (buffer == NULL || request == NULL) {
        snprintf(error, error_size, "request parse arguments are invalid");
        return 0;
    }

    memset(request, 0, sizeof(*request));

    if (http_find_header_end(buffer, buffer_length, &header_length) == NULL) {
        snprintf(error, error_size, "request headers are incomplete");
        return 0;
    }

    request_line_end = http_find_crlf(buffer, buffer + header_length);
    if (request_line_end == NULL) {
        snprintf(error, error_size, "request line is missing CRLF");
        return 0;
    }

    if (!http_parse_request_line(buffer, request_line_end, request, error, error_size)) {
        http_request_free(request);
        return 0;
    }

    if (!http_parse_headers(request_line_end + 2, buffer + header_length - 2, request, error, error_size)) {
        http_request_free(request);
        return 0;
    }

    if (request->content_length > buffer_length - header_length) {
        snprintf(error, error_size, "request body is incomplete");
        http_request_free(request);
        return 0;
    }

    body_length = request->content_length;
    request->body = (char *)malloc(body_length + 1);
    if (request->body == NULL) {
        snprintf(error, error_size, "out of memory while copying request body");
        http_request_free(request);
        return 0;
    }

    if (body_length > 0) {
        memcpy(request->body, buffer + header_length, body_length);
    }
    request->body[body_length] = '\0';
    return 1;
}

void http_request_free(http_request *request) {
    if (request == NULL) {
        return;
    }

    free(request->body);
    request->body = NULL;
    request->method[0] = '\0';
    request->path[0] = '\0';
    request->version[0] = '\0';
    request->content_type[0] = '\0';
    request->content_length = 0;
}

int http_content_type_is_sql(const char *content_type) {
    const char *value_start = content_type;
    const char *value_end;
    const char *separator;

    if (content_type == NULL || content_type[0] == '\0') {
        return 1;
    }

    value_end = content_type + strlen(content_type);
    separator = strchr(content_type, ';');
    if (separator != NULL) {
        value_end = separator;
    }

    http_trim_span(&value_start, &value_end);
    if (value_start == value_end) {
        return 1;
    }

    return http_ascii_equals_ignore_case(value_start, (size_t)(value_end - value_start), "text/plain") ||
           http_ascii_equals_ignore_case(value_start, (size_t)(value_end - value_start), "application/sql");
}

int http_build_health_response(char **response_text,
                               size_t *response_length,
                               char *error,
                               size_t error_size) {
    const char *json_body =
        "{\"ok\":true,\"path\":\"/health\",\"status_code\":200,\"message\":\"server is healthy\"}";

    return http_wrap_json_response(200, json_body, response_text, response_length, error, error_size);
}

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
                              size_t error_size) {
    char *escaped_path = NULL;
    char *escaped_error_code = NULL;
    char *escaped_message = NULL;
    char *escaped_output = NULL;
    char *json_body = NULL;
    int json_bytes;
    int ok_value = ok ? 1 : 0;
    int success = 0;

    if (!http_json_escape(path == NULL ? "" : path, &escaped_path, error, error_size) ||
        !http_json_escape(message == NULL ? "" : message, &escaped_message, error, error_size) ||
        !http_json_escape(output_text == NULL ? "" : output_text, &escaped_output, error, error_size)) {
        goto cleanup;
    }

    if (!ok_value) {
        if (!http_json_escape(error_code == NULL ? "query_failed" : error_code, &escaped_error_code, error, error_size)) {
            goto cleanup;
        }

        json_bytes = snprintf(NULL,
                              0,
                              "{\"ok\":false,\"path\":\"%s\",\"status_code\":%d,"
                              "\"error_code\":\"%s\",\"message\":\"%s\","
                              "\"affected_rows\":%d,\"output_text\":\"%s\"}",
                              escaped_path,
                              status_code,
                              escaped_error_code,
                              escaped_message,
                              affected_rows,
                              escaped_output);
    } else {
        json_bytes = snprintf(NULL,
                              0,
                              "{\"ok\":true,\"path\":\"%s\",\"status_code\":%d,"
                              "\"message\":\"%s\",\"affected_rows\":%d,"
                              "\"output_text\":\"%s\"}",
                              escaped_path,
                              status_code,
                              escaped_message,
                              affected_rows,
                              escaped_output);
    }

    if (json_bytes < 0) {
        snprintf(error, error_size, "failed to calculate query JSON payload size");
        goto cleanup;
    }

    json_body = (char *)malloc((size_t)json_bytes + 1);
    if (json_body == NULL) {
        snprintf(error, error_size, "out of memory while building query JSON payload");
        goto cleanup;
    }

    if (!ok_value) {
        snprintf(json_body,
                 (size_t)json_bytes + 1,
                 "{\"ok\":false,\"path\":\"%s\",\"status_code\":%d,"
                 "\"error_code\":\"%s\",\"message\":\"%s\","
                 "\"affected_rows\":%d,\"output_text\":\"%s\"}",
                 escaped_path,
                 status_code,
                 escaped_error_code,
                 escaped_message,
                 affected_rows,
                 escaped_output);
    } else {
        snprintf(json_body,
                 (size_t)json_bytes + 1,
                 "{\"ok\":true,\"path\":\"%s\",\"status_code\":%d,"
                 "\"message\":\"%s\",\"affected_rows\":%d,"
                 "\"output_text\":\"%s\"}",
                 escaped_path,
                 status_code,
                 escaped_message,
                 affected_rows,
                 escaped_output);
    }

    success = http_wrap_json_response(status_code, json_body, response_text, response_length, error, error_size);

cleanup:
    free(escaped_path);
    free(escaped_error_code);
    free(escaped_message);
    free(escaped_output);
    free(json_body);
    return success;
}

int http_build_error_response(int status_code,
                              const char *path,
                              const char *error_code,
                              const char *message,
                              char **response_text,
                              size_t *response_length,
                              char *error,
                              size_t error_size) {
    char *escaped_path = NULL;
    char *escaped_error_code = NULL;
    char *escaped_message = NULL;
    char *json_body = NULL;
    int json_bytes;
    int success = 0;

    if (!http_json_escape(path == NULL ? "" : path, &escaped_path, error, error_size) ||
        !http_json_escape(error_code == NULL ? "internal_error" : error_code, &escaped_error_code, error, error_size) ||
        !http_json_escape(message == NULL ? "" : message, &escaped_message, error, error_size)) {
        goto cleanup;
    }

    json_bytes = snprintf(NULL,
                          0,
                          "{\"ok\":false,\"path\":\"%s\",\"status_code\":%d,"
                          "\"error_code\":\"%s\",\"message\":\"%s\"}",
                          escaped_path,
                          status_code,
                          escaped_error_code,
                          escaped_message);
    if (json_bytes < 0) {
        snprintf(error, error_size, "failed to calculate error JSON payload size");
        goto cleanup;
    }

    json_body = (char *)malloc((size_t)json_bytes + 1);
    if (json_body == NULL) {
        snprintf(error, error_size, "out of memory while building error JSON payload");
        goto cleanup;
    }

    snprintf(json_body,
             (size_t)json_bytes + 1,
             "{\"ok\":false,\"path\":\"%s\",\"status_code\":%d,"
             "\"error_code\":\"%s\",\"message\":\"%s\"}",
             escaped_path,
             status_code,
             escaped_error_code,
             escaped_message);

    success = http_wrap_json_response(status_code, json_body, response_text, response_length, error, error_size);

cleanup:
    free(escaped_path);
    free(escaped_error_code);
    free(escaped_message);
    free(json_body);
    return success;
}

void http_response_free(char *response_text) {
    free(response_text);
}
