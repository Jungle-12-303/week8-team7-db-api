#include "server_runtime/db_server_runtime.h"
#include "server_runtime/server_main.h"

int main(int argc, char *argv[])
{
    DbServerRuntime runtime;
    ServerRuntimeHooks hooks;

    db_server_runtime_init(&runtime);

    hooks.start = db_server_runtime_start;
    hooks.wait = db_server_runtime_wait;
    hooks.request_stop = db_server_runtime_request_stop;

    return server_main(argc, argv, &hooks, &runtime);
}
