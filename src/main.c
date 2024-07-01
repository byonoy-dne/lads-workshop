// #include <open62541/plugin/log_stdout.h>
#include "namespace_app_generated.h"
#include "open62541.h"

#include <signal.h>

static volatile UA_Boolean running = true;

static void stopHandler(int sig)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "received ctrl-c");
    running = false;
}

int main(int argc, char* argv[])
{
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server* server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    if (namespace_app_generated(server) != UA_STATUSCODE_GOOD) {
        UA_Server_delete(server);
        return EXIT_FAILURE;
    }

    UA_StatusCode result = UA_Server_run(server, &running);
    UA_Server_delete(server);

    return result == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
