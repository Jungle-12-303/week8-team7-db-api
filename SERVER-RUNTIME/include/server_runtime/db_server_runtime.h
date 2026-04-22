#ifndef SERVER_RUNTIME_DB_SERVER_RUNTIME_H
#define SERVER_RUNTIME_DB_SERVER_RUNTIME_H

#include "server_runtime/runtime_config.h"

#include "engine_api.h"
#include "http_server.h"
#include "thread_pool.h"

#include <signal.h>
#include <stddef.h>

typedef struct {
    ServerRuntimeConfig config;
    server_core_engine engine;
    http_server_config http_config;
    http_server_dependencies http_dependencies;
    ConcurrencyThreadPool thread_pool;
    volatile sig_atomic_t stop_requested;
    int listener_fd;
    int thread_pool_started;
} DbServerRuntime;

void db_server_runtime_init(DbServerRuntime *runtime);
int db_server_runtime_start(const ServerRuntimeConfig *config, void *server_context, char *error, size_t error_size);
int db_server_runtime_wait(void *server_context, char *error, size_t error_size);
void db_server_runtime_request_stop(void *server_context);

#endif
