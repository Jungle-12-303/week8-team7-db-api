#include "server_runtime/server_main.h"

#include <stdio.h>

static int validate_hooks(const ServerRuntimeHooks *hooks, char *error, size_t error_size) {
    if (hooks == NULL) {
        snprintf(error, error_size, "server runtime hooks are required");
        return 0;
    }

    if (hooks->start == NULL) {
        snprintf(error, error_size, "server runtime start hook is required");
        return 0;
    }

    if (hooks->wait == NULL) {
        snprintf(error, error_size, "server runtime wait hook is required");
        return 0;
    }

    return 1;
}

static void print_runtime_summary(const ServerRuntimeConfig *config) {
    fprintf(stdout, "Starting server runtime\n");
    fprintf(stdout, "  port: %d\n", config->port);
    fprintf(stdout, "  workers: %d\n", config->worker_count);
    fprintf(stdout, "  schema_dir: %s\n", config->schema_dir);
    fprintf(stdout, "  data_dir: %s\n", config->data_dir);
    if (config->repo_root[0] != '\0') {
        fprintf(stdout, "  repo_root: %s\n", config->repo_root);
    }
    fflush(stdout);
}

int server_main(int argc, char *argv[], const ServerRuntimeHooks *hooks, void *server_context) {
    ServerRuntimeConfig config;
    char error[256];
    const char *program_name = argc > 0 && argv != NULL && argv[0] != NULL ? argv[0] : "db_server";

    if (!validate_hooks(hooks, error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    if (!server_runtime_config_load(argc, argv, &config, error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        server_runtime_print_usage(stderr, program_name);
        return 1;
    }

    if (config.show_help) {
        server_runtime_print_usage(stdout, program_name);
        return 0;
    }

    print_runtime_summary(&config);

    if (!hooks->start(&config, server_context, error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    if (!hooks->wait(server_context, error, sizeof(error))) {
        if (hooks->request_stop != NULL) {
            hooks->request_stop(server_context);
        }
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    fprintf(stdout, "Server runtime stopped cleanly\n");
    return 0;
}
