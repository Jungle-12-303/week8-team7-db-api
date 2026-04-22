#include "engine_api.h"
#include "sqlparser/common/util.h"
#include "sqlparser/execution/executor.h"
#include "sqlparser/index/table_index.h"
#include "sqlparser/sql/ast.h"
#include "sqlparser/sql/lexer.h"
#include "sqlparser/sql/parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MAKE_DIR(path) mkdir(path, 0755)
#endif

typedef struct {
    int ok;
    int affected_rows;
    char message[256];
    char *output_text;
} AdapterResult;

static int tests_run = 0;
static int tests_failed = 0;
static int temp_dir_counter = 0;

static void build_child_path(char *buffer, size_t size, const char *root, const char *child) {
    snprintf(buffer, size, "%s/%s", root, child);
}

static void expect_true(int condition, const char *name) {
    tests_run++;
    if (!condition) {
        tests_failed++;
        fprintf(stderr, "[FAIL] %s\n", name);
        return;
    }

    printf("[PASS] %s\n", name);
}

static int ensure_dir(const char *path) {
    if (MAKE_DIR(path) == 0) {
        return 1;
    }

    return errno == EEXIST;
}

static int write_text_file(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }

    if (fputs(content, file) == EOF) {
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

static int create_test_dirs(char *root, size_t root_size, char *schema_dir, size_t schema_size, char *data_dir, size_t data_size) {
    long suffix = (long)time(NULL);

    temp_dir_counter++;
    if (!ensure_dir("build")) {
        return 0;
    }

    snprintf(root, root_size, "build/adapter_contract_%ld_%d", suffix, temp_dir_counter);
    build_child_path(schema_dir, schema_size, root, "schema");
    build_child_path(data_dir, data_size, root, "data");

    if (!ensure_dir(root)) {
        return 0;
    }
    if (!ensure_dir(schema_dir)) {
        return 0;
    }
    if (!ensure_dir(data_dir)) {
        return 0;
    }

    return 1;
}

static int prepare_users_fixture(char *root, size_t root_size, char *schema_dir, size_t schema_size, char *data_dir, size_t data_size, char *data_path, size_t data_path_size, const char *csv_text) {
    char schema_path[256];

    if (!create_test_dirs(root, root_size, schema_dir, schema_size, data_dir, data_size)) {
        return 0;
    }

    build_child_path(schema_path, sizeof(schema_path), schema_dir, "users.meta");
    build_child_path(data_path, data_path_size, data_dir, "users.csv");

    if (!write_text_file(schema_path, "table=users\ncolumns=name,age\n")) {
        return 0;
    }

    if (!write_text_file(data_path, csv_text)) {
        return 0;
    }

    return 1;
}

static AdapterResult make_adapter_result(void) {
    AdapterResult result = {0};

    result.output_text = copy_string("");
    if (result.output_text == NULL) {
        snprintf(result.message, sizeof(result.message), "out of memory while preparing adapter result");
    }

    return result;
}

static void free_adapter_result(AdapterResult *result) {
    free(result->output_text);
    result->output_text = NULL;
}

static int run_week8_engine(
    void *user_data,
    const server_core_request *request,
    FILE *out,
    long long *affected_rows,
    char *message_buffer,
    size_t message_buffer_size
) {
    (void)user_data;

    TokenArray tokens = {0};
    ParseResult parse_result = {0};
    ExecResult exec_result = {0};
    char error[256];

    if (affected_rows != NULL) {
        *affected_rows = 0;
    }

    if (request == NULL || request->sql == NULL || out == NULL || message_buffer == NULL || message_buffer_size == 0) {
        return 0;
    }

    message_buffer[0] = '\0';

    if (!lex_sql(request->sql, &tokens, error, sizeof(error))) {
        snprintf(message_buffer, message_buffer_size, "%s", error);
        return 0;
    }

    parse_result = parse_statement(&tokens);
    if (!parse_result.ok) {
        snprintf(message_buffer, message_buffer_size, "%s", parse_result.message);
        free_tokens(&tokens);
        return 0;
    }

    exec_result = execute_statement(&parse_result.statement, request->schema_path, request->data_path, out);
    free_statement(&parse_result.statement);
    free_tokens(&tokens);

    if (affected_rows != NULL) {
        *affected_rows = exec_result.affected_rows;
    }
    snprintf(message_buffer, message_buffer_size, "%s", exec_result.message);
    return exec_result.ok != 0;
}

static AdapterResult execute_adapter_contract(const char *sql, const char *schema_dir, const char *data_dir) {
    AdapterResult result = make_adapter_result();
    server_core_engine engine = {0};
    server_core_request request = {0};
    server_core_result core_result = {0};
    server_core_status status;

    if (result.output_text == NULL) {
        return result;
    }

    engine.run = run_week8_engine;
    request.sql = sql;
    request.schema_path = schema_dir;
    request.data_path = data_dir;

    server_core_result_init(&core_result);
    status = server_core_execute(&engine, &request, &core_result);

    free(result.output_text);
    result.output_text = copy_string(core_result.output_text == NULL ? "" : core_result.output_text);
    if (result.output_text == NULL) {
        snprintf(result.message, sizeof(result.message), "out of memory while copying adapter output");
        server_core_result_free(&core_result);
        return result;
    }

    if (status != SERVER_CORE_STATUS_OK) {
        result.ok = 0;
        result.affected_rows = 0;
        snprintf(result.message,
                 sizeof(result.message),
                 "%s",
                 core_result.message == NULL ? server_core_status_string(status) : core_result.message);
        server_core_result_free(&core_result);
        return result;
    }

    result.ok = core_result.ok;
    result.affected_rows = (int)core_result.affected_rows;
    snprintf(result.message, sizeof(result.message), "%s", core_result.message == NULL ? "" : core_result.message);
    server_core_result_free(&core_result);
    return result;
}

static void reset_runtime_state(void) {
    execution_runtime_reset();
}

static void test_select_general_where_contract(void) {
    char root[160];
    char schema_dir[192];
    char data_dir[192];
    char data_path[224];
    AdapterResult result;

    reset_runtime_state();
    expect_true(prepare_users_fixture(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir), data_path, sizeof(data_path),
                                      "name,age\nAlice,20\nBob,21\nCarol,20\n"),
                "prepare general WHERE fixture");

    result = execute_adapter_contract("SELECT name FROM users WHERE age = 20;", schema_dir, data_dir);
    expect_true(result.output_text != NULL, "general WHERE returns allocated output text");
    expect_true(result.ok, "general WHERE returns success");
    expect_true(result.affected_rows == 2, "general WHERE returns matching row count");
    expect_true(strcmp(result.message, "SELECT 2") == 0, "general WHERE returns SELECT summary message");
    expect_true(strstr(result.output_text, "| name  |") != NULL, "general WHERE captures table header");
    expect_true(strstr(result.output_text, "Alice") != NULL, "general WHERE captures first matching row");
    expect_true(strstr(result.output_text, "Carol") != NULL, "general WHERE captures second matching row");
    expect_true(strstr(result.output_text, "Bob") == NULL, "general WHERE excludes non matching row from output");
    free_adapter_result(&result);
}

static void test_select_id_contract(void) {
    char root[160];
    char schema_dir[192];
    char data_dir[192];
    char data_path[224];
    AdapterResult result;

    reset_runtime_state();
    expect_true(prepare_users_fixture(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir), data_path, sizeof(data_path),
                                      "name,age\nAlice,20\nBob,21\nCarol,22\n"),
                "prepare id SELECT fixture");

    result = execute_adapter_contract("SELECT name FROM users WHERE id = 2;", schema_dir, data_dir);
    expect_true(result.output_text != NULL, "id SELECT returns allocated output text");
    expect_true(result.ok, "id SELECT returns success");
    expect_true(result.affected_rows == 1, "id SELECT returns one matching row");
    expect_true(strcmp(result.message, "SELECT 1") == 0, "id SELECT returns SELECT summary message");
    expect_true(strstr(result.output_text, "| name |") != NULL, "id SELECT captures compact table header");
    expect_true(strstr(result.output_text, "Bob") != NULL, "id SELECT captures indexed row");
    expect_true(strstr(result.output_text, "Alice") == NULL, "id SELECT excludes non matching row from output");
    expect_true(table_index_is_loaded("users"), "id SELECT leaves table index loaded");
    free_adapter_result(&result);
}

static void test_insert_contract(void) {
    char root[160];
    char schema_dir[192];
    char data_dir[192];
    char data_path[224];
    char error[256];
    char *csv_text;
    AdapterResult result;

    reset_runtime_state();
    expect_true(prepare_users_fixture(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir), data_path, sizeof(data_path),
                                      "name,age\n"),
                "prepare INSERT fixture");

    result = execute_adapter_contract("INSERT INTO users (name, age) VALUES ('Alice', 20);", schema_dir, data_dir);
    expect_true(result.output_text != NULL, "INSERT returns allocated output text");
    expect_true(result.ok, "INSERT returns success");
    expect_true(result.affected_rows == 1, "INSERT returns one affected row");
    expect_true(strcmp(result.message, "INSERT 1") == 0, "INSERT returns INSERT summary message");
    expect_true(result.output_text[0] == '\0', "INSERT leaves captured output empty");
    expect_true(table_index_is_loaded("users"), "INSERT leaves table index loaded");

    csv_text = read_entire_file(data_path, error, sizeof(error));
    expect_true(csv_text != NULL, "INSERT writes CSV row");
    if (csv_text != NULL) {
        expect_true(strstr(csv_text, "Alice,20") != NULL, "INSERT appends values to CSV");
        free(csv_text);
    }

    free_adapter_result(&result);
}

static void test_parse_error_contract(void) {
    AdapterResult result;

    reset_runtime_state();
    result = execute_adapter_contract("SELECT name users;", "unused-schema", "unused-data");
    expect_true(result.output_text != NULL, "parse error returns allocated output text");
    expect_true(!result.ok, "parse error returns failure");
    expect_true(result.affected_rows == 0, "parse error keeps affected rows at zero");
    expect_true(strstr(result.message, "expected keyword FROM") != NULL, "parse error returns parser detail");
    expect_true(result.output_text[0] == '\0', "parse error keeps captured output empty");
    free_adapter_result(&result);
}

static void test_execute_error_contract(void) {
    char root[160];
    char schema_dir[192];
    char data_dir[192];
    char data_path[224];
    AdapterResult result;

    reset_runtime_state();
    expect_true(prepare_users_fixture(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir), data_path, sizeof(data_path),
                                      "name,age\nAlice,20\n"),
                "prepare execute error fixture");

    result = execute_adapter_contract("SELECT missing FROM users;", schema_dir, data_dir);
    expect_true(result.output_text != NULL, "execute error returns allocated output text");
    expect_true(!result.ok, "execute error returns failure");
    expect_true(result.affected_rows == 0, "execute error keeps affected rows at zero");
    expect_true(strstr(result.message, "unknown column in SELECT: missing") != NULL, "execute error returns executor detail");
    expect_true(result.output_text[0] == '\0', "execute error keeps captured output empty");
    free_adapter_result(&result);
}

int main(void) {
    test_select_general_where_contract();
    test_select_id_contract();
    test_insert_contract();
    test_parse_error_contract();
    test_execute_error_contract();

    execution_runtime_reset();
    printf("Tests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
