#include "engine_api.h"

#include <stdlib.h>
#include <string.h>

#define SERVER_CORE_CAPTURE_READ_CHUNK 4096U

static void server_core_result_zero(server_core_result *result)
{
    if (result == NULL) {
        return;
    }

    result->ok = 0;
    result->affected_rows = SERVER_CORE_AFFECTED_ROWS_UNKNOWN;
    result->message = NULL;
    result->output_text = NULL;
}

static char *server_core_strdup_sized(const char *text, size_t length)
{
    char *copy;

    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

static char *server_core_strdup_or_empty(const char *text)
{
    if (text == NULL) {
        return server_core_strdup_sized("", 0U);
    }

    return server_core_strdup_sized(text, strlen(text));
}

static server_core_status server_core_set_string(char **slot, const char *text)
{
    char *copy;

    if (slot == NULL) {
        return SERVER_CORE_STATUS_INVALID_ARGUMENT;
    }

    copy = server_core_strdup_or_empty(text);
    if (copy == NULL) {
        return SERVER_CORE_STATUS_NO_MEMORY;
    }

    free(*slot);
    *slot = copy;
    return SERVER_CORE_STATUS_OK;
}

static server_core_status server_core_read_stream(FILE *stream, char **text_out)
{
    char chunk[SERVER_CORE_CAPTURE_READ_CHUNK];
    char *buffer;
    size_t used;
    size_t capacity;

    if (stream == NULL || text_out == NULL) {
        return SERVER_CORE_STATUS_INVALID_ARGUMENT;
    }

    if (fflush(stream) != 0) {
        return SERVER_CORE_STATUS_CAPTURE_ERROR;
    }

    if (fseek(stream, 0L, SEEK_SET) != 0) {
        return SERVER_CORE_STATUS_CAPTURE_ERROR;
    }

    buffer = NULL;
    used = 0U;
    capacity = 0U;

    for (;;) {
        size_t read_count;

        read_count = fread(chunk, 1U, sizeof(chunk), stream);
        if (read_count > 0U) {
            size_t required;

            required = used + read_count + 1U;
            if (required > capacity) {
                size_t new_capacity;
                char *resized;

                new_capacity = capacity == 0U ? required : capacity;
                while (new_capacity < required) {
                    new_capacity *= 2U;
                    if (new_capacity < required) {
                        new_capacity += SERVER_CORE_CAPTURE_READ_CHUNK;
                    }
                }

                resized = (char *)realloc(buffer, new_capacity);
                if (resized == NULL) {
                    free(buffer);
                    return SERVER_CORE_STATUS_NO_MEMORY;
                }

                buffer = resized;
                capacity = new_capacity;
            }

            memcpy(buffer + used, chunk, read_count);
            used += read_count;
            buffer[used] = '\0';
        }

        if (read_count < sizeof(chunk)) {
            if (ferror(stream)) {
                free(buffer);
                return SERVER_CORE_STATUS_CAPTURE_ERROR;
            }
            break;
        }
    }

    if (buffer == NULL) {
        buffer = server_core_strdup_sized("", 0U);
        if (buffer == NULL) {
            return SERVER_CORE_STATUS_NO_MEMORY;
        }
    }

    *text_out = buffer;
    return SERVER_CORE_STATUS_OK;
}

static server_core_status server_core_populate_system_error(
    server_core_result *result,
    server_core_status status,
    const char *message)
{
    server_core_status copy_status;

    if (result == NULL) {
        return status;
    }

    result->ok = 0;
    result->affected_rows = SERVER_CORE_AFFECTED_ROWS_UNKNOWN;

    copy_status = server_core_set_string(&result->message, message);
    if (copy_status != SERVER_CORE_STATUS_OK) {
        return copy_status;
    }

    copy_status = server_core_set_string(&result->output_text, "");
    if (copy_status != SERVER_CORE_STATUS_OK) {
        return copy_status;
    }

    return status;
}

void server_core_result_init(server_core_result *result)
{
    server_core_result_zero(result);
}

void server_core_result_reset(server_core_result *result)
{
    if (result == NULL) {
        return;
    }

    free(result->message);
    free(result->output_text);
    server_core_result_zero(result);
}

void server_core_result_free(server_core_result *result)
{
    server_core_result_reset(result);
}

const char *server_core_status_string(server_core_status status)
{
    switch (status) {
    case SERVER_CORE_STATUS_OK:
        return "ok";
    case SERVER_CORE_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case SERVER_CORE_STATUS_NO_MEMORY:
        return "out of memory";
    case SERVER_CORE_STATUS_CAPTURE_ERROR:
        return "output capture error";
    case SERVER_CORE_STATUS_ENGINE_NOT_CONFIGURED:
        return "engine not configured";
    default:
        return "unknown error";
    }
}

server_core_status server_core_execute(
    const server_core_engine *engine,
    const server_core_request *request,
    server_core_result *result)
{
    FILE *capture;
    server_core_status status;
    char *captured_output;
    char message_buffer[SERVER_CORE_MESSAGE_CAPACITY];
    long long affected_rows;
    int engine_ok;
    const char *message_text;

    if (result == NULL) {
        return SERVER_CORE_STATUS_INVALID_ARGUMENT;
    }

    server_core_result_reset(result);

    if (engine == NULL || engine->run == NULL) {
        return server_core_populate_system_error(
            result,
            SERVER_CORE_STATUS_ENGINE_NOT_CONFIGURED,
            "engine callback is not configured");
    }

    if (request == NULL || request->sql == NULL || request->sql[0] == '\0') {
        return server_core_populate_system_error(
            result,
            SERVER_CORE_STATUS_INVALID_ARGUMENT,
            "request.sql must not be empty");
    }

    capture = tmpfile();
    if (capture == NULL) {
        return server_core_populate_system_error(
            result,
            SERVER_CORE_STATUS_CAPTURE_ERROR,
            "failed to allocate capture stream");
    }

    message_buffer[0] = '\0';
    affected_rows = SERVER_CORE_AFFECTED_ROWS_UNKNOWN;
    engine_ok = engine->run(
        engine->user_data,
        request,
        capture,
        &affected_rows,
        message_buffer,
        sizeof(message_buffer));

    status = server_core_read_stream(capture, &captured_output);
    fclose(capture);
    if (status != SERVER_CORE_STATUS_OK) {
        return server_core_populate_system_error(
            result,
            status,
            "failed to read captured engine output");
    }

    result->ok = engine_ok != 0;
    result->affected_rows = affected_rows;
    result->output_text = captured_output;

    if (message_buffer[0] != '\0') {
        message_text = message_buffer;
    } else if (result->ok) {
        message_text = "ok";
    } else {
        message_text = "engine execution failed";
    }

    status = server_core_set_string(&result->message, message_text);
    if (status != SERVER_CORE_STATUS_OK) {
        server_core_result_reset(result);
        return status;
    }

    return SERVER_CORE_STATUS_OK;
}
