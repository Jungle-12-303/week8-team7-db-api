#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
RUNTIME_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$RUNTIME_DIR/.." && pwd)

SERVER_BIN=${DB_SERVER_BIN:-"$REPO_ROOT/build/bin/db_server"}
PORT=${DB_SERVER_PORT:-8080}
WORKERS=${DB_SERVER_WORKERS:-4}
SCHEMA_DIR=${DB_SCHEMA_DIR:-"$REPO_ROOT/week8-team7-db-api/schema"}
DATA_DIR=${DB_DATA_DIR:-"$REPO_ROOT/week8-team7-db-api/data"}

if [ ! -x "$SERVER_BIN" ]; then
  printf '%s\n' "error: server binary not found or not executable: $SERVER_BIN" >&2
  exit 1
fi

exec "$SERVER_BIN" "$PORT" "$WORKERS" --schema-dir "$SCHEMA_DIR" --data-dir "$DATA_DIR" "$@"
