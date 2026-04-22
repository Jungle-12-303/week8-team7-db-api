#ifndef SERVER_CORE_WEEK8_ENGINE_H
#define SERVER_CORE_WEEK8_ENGINE_H

#include "engine_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int server_core_week8_engine_run(
    void *user_data,
    const server_core_request *request,
    FILE *out,
    long long *affected_rows,
    char *message_buffer,
    size_t message_buffer_size);

void server_core_week8_engine_reset(void);

#ifdef __cplusplus
}
#endif

#endif
