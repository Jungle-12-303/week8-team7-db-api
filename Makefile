CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
INCLUDES = \
	-ISERVER-CONCURRENCY/include \
	-ISERVER-CORE/include \
	-ISERVER-HTTP/include \
	-ISERVER-RUNTIME/include \
	-Iweek8-team7-db-api/include

BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

SERVER_BIN = $(BIN_DIR)/db_server

SERVER_SOURCES = \
	SERVER-CONCURRENCY/src/job_queue.c \
	SERVER-CONCURRENCY/src/lock_manager.c \
	SERVER-CONCURRENCY/src/thread_pool.c \
	SERVER-CORE/src/engine_api.c \
	SERVER-CORE/src/week8_engine.c \
	SERVER-HTTP/src/http_protocol.c \
	SERVER-HTTP/src/http_server.c \
	SERVER-RUNTIME/src/runtime_config.c \
	SERVER-RUNTIME/src/server_main.c \
	SERVER-RUNTIME/src/db_server_runtime.c \
	SERVER-RUNTIME/src/db_server_main.c \
	week8-team7-db-api/src/common/util.c \
	week8-team7-db-api/src/storage/schema.c \
	week8-team7-db-api/src/storage/storage.c \
	week8-team7-db-api/src/sql/ast.c \
	week8-team7-db-api/src/sql/lexer.c \
	week8-team7-db-api/src/sql/parser.c \
	week8-team7-db-api/src/execution/executor.c \
	week8-team7-db-api/src/index/bptree.c \
	week8-team7-db-api/src/index/table_index.c

.PHONY: all server engine-test clean

all: server

server: $(SERVER_BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SERVER_BIN): $(SERVER_SOURCES) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SERVER_SOURCES)

engine-test:
	$(MAKE) -C week8-team7-db-api test

clean:
	rm -rf $(BUILD_DIR)
