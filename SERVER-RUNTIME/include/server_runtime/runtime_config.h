#ifndef SERVER_RUNTIME_RUNTIME_CONFIG_H
#define SERVER_RUNTIME_RUNTIME_CONFIG_H

#include <stddef.h>
#include <stdio.h>

#define SERVER_RUNTIME_PATH_MAX 1024
#define SERVER_RUNTIME_DEFAULT_PORT 8080
#define SERVER_RUNTIME_DEFAULT_WORKER_COUNT 4

#define SERVER_RUNTIME_ENV_PORT "DB_SERVER_PORT"
#define SERVER_RUNTIME_ENV_WORKERS "DB_SERVER_WORKERS"
#define SERVER_RUNTIME_ENV_REPO_ROOT "DB_SERVER_REPO_ROOT"
#define SERVER_RUNTIME_ENV_SCHEMA_DIR "DB_SCHEMA_DIR"
#define SERVER_RUNTIME_ENV_DATA_DIR "DB_DATA_DIR"

typedef struct {
    int port;
    int worker_count;
    int show_help;
    char program_path[SERVER_RUNTIME_PATH_MAX];
    char working_directory[SERVER_RUNTIME_PATH_MAX];
    char repo_root[SERVER_RUNTIME_PATH_MAX];
    char schema_dir[SERVER_RUNTIME_PATH_MAX];
    char data_dir[SERVER_RUNTIME_PATH_MAX];
} ServerRuntimeConfig;

void server_runtime_config_init(ServerRuntimeConfig *config);
int server_runtime_config_load(int argc, char *argv[], ServerRuntimeConfig *config, char *error, size_t error_size);
void server_runtime_print_usage(FILE *stream, const char *program_name);

#endif
