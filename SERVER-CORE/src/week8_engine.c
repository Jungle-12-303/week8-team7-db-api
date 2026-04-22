#include "week8_engine.h"

#include "sqlparser/execution/executor.h"
#include "sqlparser/sql/ast.h"
#include "sqlparser/sql/lexer.h"
#include "sqlparser/sql/parser.h"

#include <stdio.h>
#include <string.h>

int server_core_week8_engine_run(
    void *user_data,
    const server_core_request *request,
    FILE *out,
    long long *affected_rows,
    char *message_buffer,
    size_t message_buffer_size)
{
    TokenArray tokens = {0};
    ParseResult parse_result = {0};
    ExecResult exec_result = {0};
    char error[256];

    (void)user_data;

    if (request == NULL || request->sql == NULL || request->schema_path == NULL ||
        request->data_path == NULL || out == NULL || message_buffer == NULL ||
        message_buffer_size == 0U) {
        return 0;
    }

    if (affected_rows != NULL) {
        *affected_rows = 0;
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

void server_core_week8_engine_reset(void)
{
    execution_runtime_reset();
}
