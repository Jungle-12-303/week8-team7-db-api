#!/usr/bin/env sh
set -eu

HOST=${DB_SERVER_HOST:-127.0.0.1}
PORT=${DB_SERVER_PORT:-8080}
URL="http://$HOST:$PORT/health"

if ! command -v curl >/dev/null 2>&1; then
  printf '%s\n' "error: curl is required for smoke-health.sh" >&2
  exit 1
fi

curl --fail --silent --show-error "$URL"
printf '\n'
