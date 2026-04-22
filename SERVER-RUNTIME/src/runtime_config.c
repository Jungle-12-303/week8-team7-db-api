#include "server_runtime/runtime_config.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <direct.h>
#include <sys/stat.h>
#define GETCWD _getcwd
#define STAT_STRUCT struct _stat
#define STAT_FUNC _stat
#define IS_DIRECTORY(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <sys/stat.h>
#include <unistd.h>
#define GETCWD getcwd
#define STAT_STRUCT struct stat
#define STAT_FUNC stat
#define IS_DIRECTORY(mode) S_ISDIR(mode)
#endif

#define PATH_SEP '/'

typedef struct {
    int port_set;
    int worker_count_set;
    int repo_root_set;
    int schema_dir_set;
    int data_dir_set;
} ServerRuntimeOverrideState;

static int copy_text(char *destination, size_t destination_size, const char *source, char *error, size_t error_size) {
    size_t length;

    if (destination == NULL || destination_size == 0) {
        snprintf(error, error_size, "internal error: invalid destination buffer");
        return 0;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return 1;
    }

    length = strlen(source);
    if (length >= destination_size) {
        snprintf(error, error_size, "path is too long: %s", source);
        return 0;
    }

    memcpy(destination, source, length + 1);
    return 1;
}

static int parse_int_value(const char *text, int *value) {
    char *end = NULL;
    long parsed;

    if (text == NULL || value == NULL || *text == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }

    if (parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static int path_is_absolute(const char *path) {
    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }

    return path[0] != '\0' &&
           path[1] == ':' &&
           (path[2] == '\0' || path[2] == '/' || path[2] == '\\');
}

static void trim_trailing_separators(char *path) {
    size_t length;

    if (path == NULL) {
        return;
    }

    length = strlen(path);
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) {
        if (length == 3 && path[1] == ':') {
            break;
        }

        path[length - 1] = '\0';
        length--;
    }
}

static int join_path(char *buffer, size_t buffer_size, const char *left, const char *right, char *error, size_t error_size) {
    size_t left_length;
    size_t right_offset = 0;

    if (left == NULL || right == NULL) {
        snprintf(error, error_size, "internal error: missing path component");
        return 0;
    }

    left_length = strlen(left);
    while (right[right_offset] == '/' || right[right_offset] == '\\') {
        right_offset++;
    }

    if (left_length > 0 && left[left_length - 1] != '/' && left[left_length - 1] != '\\') {
        if (snprintf(buffer, buffer_size, "%s%c%s", left, PATH_SEP, right + right_offset) >= (int)buffer_size) {
            snprintf(error, error_size, "path is too long while joining '%s' and '%s'", left, right);
            return 0;
        }
        return 1;
    }

    if (snprintf(buffer, buffer_size, "%s%s", left, right + right_offset) >= (int)buffer_size) {
        snprintf(error, error_size, "path is too long while joining '%s' and '%s'", left, right);
        return 0;
    }

    return 1;
}

static int directory_exists(const char *path) {
    STAT_STRUCT info;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (STAT_FUNC(path, &info) != 0) {
        return 0;
    }

    return IS_DIRECTORY(info.st_mode) != 0;
}

static int file_exists(const char *path) {
    STAT_STRUCT info;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    return STAT_FUNC(path, &info) == 0;
}

static int get_current_working_directory(char *buffer, size_t buffer_size, char *error, size_t error_size) {
    if (GETCWD(buffer, (int)buffer_size) == NULL) {
        snprintf(error, error_size, "failed to resolve current working directory");
        return 0;
    }

    trim_trailing_separators(buffer);
    return 1;
}

static int parent_directory(char *path) {
    size_t length;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    trim_trailing_separators(path);
    length = strlen(path);

    while (length > 0 && path[length - 1] != '/' && path[length - 1] != '\\') {
        length--;
    }

    if (length == 0) {
        return 0;
    }

    if (length == 3 && path[1] == ':') {
        path[length] = '\0';
        return 0;
    }

    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) {
        length--;
    }

    path[length] = '\0';
    return 1;
}

static int is_repo_root(const char *candidate, char *scratch, size_t scratch_size, char *error, size_t error_size) {
    if (!join_path(scratch, scratch_size, candidate, "PLAN.md", error, error_size)) {
        return 0;
    }
    if (!file_exists(scratch)) {
        return 0;
    }

    if (!join_path(scratch, scratch_size, candidate, "SERVER-RUNTIME", error, error_size)) {
        return 0;
    }
    if (!directory_exists(scratch)) {
        return 0;
    }

    if (!join_path(scratch, scratch_size, candidate, "week8-team7-db-api", error, error_size)) {
        return 0;
    }
    if (!directory_exists(scratch)) {
        return 0;
    }

    return 1;
}

static int find_repo_root_from_start(const char *start_path, char *repo_root, size_t repo_root_size, char *error, size_t error_size) {
    char current[SERVER_RUNTIME_PATH_MAX];
    char scratch[SERVER_RUNTIME_PATH_MAX];

    if (start_path == NULL || *start_path == '\0') {
        return 0;
    }

    if (!copy_text(current, sizeof(current), start_path, error, error_size)) {
        return 0;
    }

    trim_trailing_separators(current);
    while (current[0] != '\0') {
        if (is_repo_root(current, scratch, sizeof(scratch), error, error_size)) {
            return copy_text(repo_root, repo_root_size, current, error, error_size);
        }

        if (!parent_directory(current)) {
            break;
        }
    }

    return 0;
}

static int resolve_program_directory(const char *program_path,
                                     const char *working_directory,
                                     char *program_directory,
                                     size_t program_directory_size,
                                     char *error,
                                     size_t error_size) {
    char absolute_path[SERVER_RUNTIME_PATH_MAX];

    if (program_path == NULL || *program_path == '\0') {
        program_directory[0] = '\0';
        return 1;
    }

    if (strchr(program_path, '/') == NULL && strchr(program_path, '\\') == NULL) {
        program_directory[0] = '\0';
        return 1;
    }

    if (path_is_absolute(program_path)) {
        if (!copy_text(absolute_path, sizeof(absolute_path), program_path, error, error_size)) {
            return 0;
        }
    } else if (!join_path(absolute_path, sizeof(absolute_path), working_directory, program_path, error, error_size)) {
        return 0;
    }

    if (!copy_text(program_directory, program_directory_size, absolute_path, error, error_size)) {
        return 0;
    }

    if (!parent_directory(program_directory)) {
        program_directory[0] = '\0';
    }

    return 1;
}

static void apply_port_if_valid(ServerRuntimeConfig *config, int port) {
    config->port = port;
}

static void apply_worker_count_if_valid(ServerRuntimeConfig *config, int worker_count) {
    config->worker_count = worker_count;
}

static int apply_directory_override(char *destination,
                                    size_t destination_size,
                                    const char *value,
                                    char *error,
                                    size_t error_size) {
    if (value == NULL || *value == '\0') {
        snprintf(error, error_size, "empty directory path is not allowed");
        return 0;
    }

    return copy_text(destination, destination_size, value, error, error_size);
}

static int load_env_overrides(ServerRuntimeConfig *config,
                              ServerRuntimeOverrideState *state,
                              char *error,
                              size_t error_size) {
    const char *value;
    int parsed_value;

    value = getenv(SERVER_RUNTIME_ENV_PORT);
    if (value != NULL && *value != '\0') {
        if (!parse_int_value(value, &parsed_value)) {
            snprintf(error, error_size, "invalid %s value: %s", SERVER_RUNTIME_ENV_PORT, value);
            return 0;
        }
        apply_port_if_valid(config, parsed_value);
        state->port_set = 1;
    }

    value = getenv(SERVER_RUNTIME_ENV_WORKERS);
    if (value != NULL && *value != '\0') {
        if (!parse_int_value(value, &parsed_value)) {
            snprintf(error, error_size, "invalid %s value: %s", SERVER_RUNTIME_ENV_WORKERS, value);
            return 0;
        }
        apply_worker_count_if_valid(config, parsed_value);
        state->worker_count_set = 1;
    }

    value = getenv(SERVER_RUNTIME_ENV_REPO_ROOT);
    if (value != NULL && *value != '\0') {
        if (!apply_directory_override(config->repo_root, sizeof(config->repo_root), value, error, error_size)) {
            return 0;
        }
        state->repo_root_set = 1;
    }

    value = getenv(SERVER_RUNTIME_ENV_SCHEMA_DIR);
    if (value != NULL && *value != '\0') {
        if (!apply_directory_override(config->schema_dir, sizeof(config->schema_dir), value, error, error_size)) {
            return 0;
        }
        state->schema_dir_set = 1;
    }

    value = getenv(SERVER_RUNTIME_ENV_DATA_DIR);
    if (value != NULL && *value != '\0') {
        if (!apply_directory_override(config->data_dir, sizeof(config->data_dir), value, error, error_size)) {
            return 0;
        }
        state->data_dir_set = 1;
    }

    return 1;
}

static int require_argument(int argc, char *argv[], int *index, const char *option, char **value, char *error, size_t error_size) {
    if (*index + 1 >= argc) {
        snprintf(error, error_size, "missing value after %s", option);
        return 0;
    }

    *index += 1;
    *value = argv[*index];
    return 1;
}

static int load_cli_overrides(int argc,
                              char *argv[],
                              ServerRuntimeConfig *config,
                              ServerRuntimeOverrideState *state,
                              char *error,
                              size_t error_size) {
    int index;
    int positional_count = 0;

    for (index = 1; index < argc; index++) {
        char *argument = argv[index];
        char *value = NULL;
        int parsed_value;

        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            config->show_help = 1;
            continue;
        }

        if (strcmp(argument, "--port") == 0) {
            if (!require_argument(argc, argv, &index, argument, &value, error, error_size)) {
                return 0;
            }
            if (!parse_int_value(value, &parsed_value)) {
                snprintf(error, error_size, "invalid port value: %s", value);
                return 0;
            }
            apply_port_if_valid(config, parsed_value);
            state->port_set = 1;
            continue;
        }

        if (strcmp(argument, "--workers") == 0 || strcmp(argument, "--worker-count") == 0) {
            if (!require_argument(argc, argv, &index, argument, &value, error, error_size)) {
                return 0;
            }
            if (!parse_int_value(value, &parsed_value)) {
                snprintf(error, error_size, "invalid worker count value: %s", value);
                return 0;
            }
            apply_worker_count_if_valid(config, parsed_value);
            state->worker_count_set = 1;
            continue;
        }

        if (strcmp(argument, "--repo-root") == 0) {
            if (!require_argument(argc, argv, &index, argument, &value, error, error_size)) {
                return 0;
            }
            if (!apply_directory_override(config->repo_root, sizeof(config->repo_root), value, error, error_size)) {
                return 0;
            }
            state->repo_root_set = 1;
            continue;
        }

        if (strcmp(argument, "--schema-dir") == 0) {
            if (!require_argument(argc, argv, &index, argument, &value, error, error_size)) {
                return 0;
            }
            if (!apply_directory_override(config->schema_dir, sizeof(config->schema_dir), value, error, error_size)) {
                return 0;
            }
            state->schema_dir_set = 1;
            continue;
        }

        if (strcmp(argument, "--data-dir") == 0) {
            if (!require_argument(argc, argv, &index, argument, &value, error, error_size)) {
                return 0;
            }
            if (!apply_directory_override(config->data_dir, sizeof(config->data_dir), value, error, error_size)) {
                return 0;
            }
            state->data_dir_set = 1;
            continue;
        }

        if (argument[0] == '-' && argument[1] != '\0') {
            snprintf(error, error_size, "unknown option: %s", argument);
            return 0;
        }

        if (positional_count == 0) {
            if (!parse_int_value(argument, &parsed_value)) {
                snprintf(error, error_size, "invalid port value: %s", argument);
                return 0;
            }
            apply_port_if_valid(config, parsed_value);
            state->port_set = 1;
            positional_count++;
            continue;
        }

        if (positional_count == 1) {
            if (!parse_int_value(argument, &parsed_value)) {
                snprintf(error, error_size, "invalid worker count value: %s", argument);
                return 0;
            }
            apply_worker_count_if_valid(config, parsed_value);
            state->worker_count_set = 1;
            positional_count++;
            continue;
        }

        snprintf(error, error_size, "unexpected extra argument: %s", argument);
        return 0;
    }

    return 1;
}

static int resolve_repo_root(ServerRuntimeConfig *config, char *error, size_t error_size) {
    char program_directory[SERVER_RUNTIME_PATH_MAX];

    if (config->repo_root[0] != '\0') {
        trim_trailing_separators(config->repo_root);
        return 1;
    }

    if (find_repo_root_from_start(config->working_directory, config->repo_root, sizeof(config->repo_root), error, error_size)) {
        return 1;
    }

    if (!resolve_program_directory(config->program_path,
                                   config->working_directory,
                                   program_directory,
                                   sizeof(program_directory),
                                   error,
                                   error_size)) {
        return 0;
    }

    if (program_directory[0] != '\0' &&
        find_repo_root_from_start(program_directory, config->repo_root, sizeof(config->repo_root), error, error_size)) {
        return 1;
    }

    return 1;
}

static int derive_default_paths(ServerRuntimeConfig *config,
                                const ServerRuntimeOverrideState *state,
                                char *error,
                                size_t error_size) {
    char project_root[SERVER_RUNTIME_PATH_MAX];

    if (config->repo_root[0] == '\0') {
        if (!state->schema_dir_set || !state->data_dir_set) {
            snprintf(error,
                     error_size,
                     "failed to infer repo root; set %s or pass --repo-root/--schema-dir/--data-dir",
                     SERVER_RUNTIME_ENV_REPO_ROOT);
            return 0;
        }
        return 1;
    }

    if (!join_path(project_root, sizeof(project_root), config->repo_root, "week8-team7-db-api", error, error_size)) {
        return 0;
    }

    if (!state->schema_dir_set &&
        !join_path(config->schema_dir, sizeof(config->schema_dir), project_root, "schema", error, error_size)) {
        return 0;
    }

    if (!state->data_dir_set &&
        !join_path(config->data_dir, sizeof(config->data_dir), project_root, "data", error, error_size)) {
        return 0;
    }

    return 1;
}

static int validate_config(const ServerRuntimeConfig *config, char *error, size_t error_size) {
    if (config->port < 1 || config->port > 65535) {
        snprintf(error, error_size, "port must be between 1 and 65535");
        return 0;
    }

    if (config->worker_count < 1) {
        snprintf(error, error_size, "worker count must be at least 1");
        return 0;
    }

    if (config->schema_dir[0] == '\0') {
        snprintf(error, error_size, "schema directory is not configured");
        return 0;
    }

    if (config->data_dir[0] == '\0') {
        snprintf(error, error_size, "data directory is not configured");
        return 0;
    }

    if (!directory_exists(config->schema_dir)) {
        snprintf(error, error_size, "schema directory does not exist: %s", config->schema_dir);
        return 0;
    }

    if (!directory_exists(config->data_dir)) {
        snprintf(error, error_size, "data directory does not exist: %s", config->data_dir);
        return 0;
    }

    if (config->repo_root[0] != '\0' && !directory_exists(config->repo_root)) {
        snprintf(error, error_size, "repo root does not exist: %s", config->repo_root);
        return 0;
    }

    return 1;
}

void server_runtime_config_init(ServerRuntimeConfig *config) {
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->port = SERVER_RUNTIME_DEFAULT_PORT;
    config->worker_count = SERVER_RUNTIME_DEFAULT_WORKER_COUNT;
}

int server_runtime_config_load(int argc, char *argv[], ServerRuntimeConfig *config, char *error, size_t error_size) {
    ServerRuntimeOverrideState state;

    if (config == NULL || error == NULL || error_size == 0) {
        return 0;
    }

    server_runtime_config_init(config);
    memset(&state, 0, sizeof(state));
    error[0] = '\0';

    if (argc > 0 && argv != NULL && argv[0] != NULL) {
        if (!copy_text(config->program_path, sizeof(config->program_path), argv[0], error, error_size)) {
            return 0;
        }
    }

    if (!get_current_working_directory(config->working_directory, sizeof(config->working_directory), error, error_size)) {
        return 0;
    }

    if (!load_env_overrides(config, &state, error, error_size)) {
        return 0;
    }

    if (!load_cli_overrides(argc, argv, config, &state, error, error_size)) {
        return 0;
    }

    if (config->show_help) {
        return 1;
    }

    if (!resolve_repo_root(config, error, error_size)) {
        return 0;
    }

    if (!derive_default_paths(config, &state, error, error_size)) {
        return 0;
    }

    return validate_config(config, error, error_size);
}

void server_runtime_print_usage(FILE *stream, const char *program_name) {
    const char *name = program_name != NULL && *program_name != '\0' ? program_name : "db_server";

    fprintf(stream, "Usage: %s <port> [worker_count] [options]\n", name);
    fprintf(stream, "       %s [options]\n", name);
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  --port <port>            Override listen port (default: %d)\n", SERVER_RUNTIME_DEFAULT_PORT);
    fprintf(stream, "  --workers <count>        Override worker count (default: %d)\n", SERVER_RUNTIME_DEFAULT_WORKER_COUNT);
    fprintf(stream, "  --repo-root <path>       Root directory that contains PLAN.md and week8-team7-db-api/\n");
    fprintf(stream, "  --schema-dir <path>      Explicit schema directory\n");
    fprintf(stream, "  --data-dir <path>        Explicit data directory\n");
    fprintf(stream, "  -h, --help               Show this help message\n");
    fprintf(stream, "\n");
    fprintf(stream, "Environment:\n");
    fprintf(stream, "  %s, %s\n", SERVER_RUNTIME_ENV_PORT, SERVER_RUNTIME_ENV_WORKERS);
    fprintf(stream, "  %s, %s, %s\n", SERVER_RUNTIME_ENV_REPO_ROOT, SERVER_RUNTIME_ENV_SCHEMA_DIR, SERVER_RUNTIME_ENV_DATA_DIR);
    fprintf(stream, "\n");
    fprintf(stream, "Default path resolution:\n");
    fprintf(stream, "  <repo_root>/week8-team7-db-api/schema\n");
    fprintf(stream, "  <repo_root>/week8-team7-db-api/data\n");
}
