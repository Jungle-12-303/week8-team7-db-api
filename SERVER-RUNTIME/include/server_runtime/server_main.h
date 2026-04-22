#ifndef SERVER_RUNTIME_SERVER_MAIN_H
#define SERVER_RUNTIME_SERVER_MAIN_H

#include "server_runtime/runtime_config.h"

#include <stddef.h>

typedef struct {
    int (*start)(const ServerRuntimeConfig *config, void *server_context, char *error, size_t error_size);
    int (*wait)(void *server_context, char *error, size_t error_size);
    void (*request_stop)(void *server_context);
} ServerRuntimeHooks;

int server_main(int argc, char *argv[], const ServerRuntimeHooks *hooks, void *server_context);

#endif
