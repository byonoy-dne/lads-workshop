#include "namespace_app_generated.h"
#include "open62541.h"

#include <signal.h>

#define workshopStep 13

#define UA_CHECK(status)                                                                                               \
    do {                                                                                                               \
        if (status != UA_STATUSCODE_GOOD) {                                                                            \
            UA_Server_delete(server);                                                                                  \
            return EXIT_FAILURE;                                                                                       \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)

static volatile UA_Boolean running = true;

static void stopHandler(int sig)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received CTRL+C");
    running = false;
}

UA_StatusCode runServer(UA_Server* server)
{
    while (running) {
        UA_Server_run_iterate(server, true);
    }

    UA_StatusCode result = UA_Server_run_shutdown(server);
    UA_Server_delete(server);

    return result;
}

int main(int argc, char* argv[])
{
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    //---------------------------------------------------------------
    // Step 1: load the required OPC UA nodesets and start the server
    //---------------------------------------------------------------

    UA_Server* server = UA_Server_new();

    UA_ServerConfig* config = UA_Server_getConfig(server);
    UA_ServerConfig_setMinimal(config, 26543, NULL);

    const char* uri = "LADS-SampleServer";
    config->buildInfo.manufacturerName = UA_STRING("SPECTARIS");
    config->buildInfo.productUri = UA_STRING_ALLOC(uri);
    config->buildInfo.softwareVersion = UA_STRING("1.0.0");
    config->applicationDescription.applicationName = UA_LOCALIZEDTEXT_ALLOC("en", "LADS LuminescenceReader");
    config->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;
    config->applicationDescription.productUri = UA_STRING_ALLOC(uri);
    config->applicationDescription.applicationUri = UA_STRING_ALLOC(uri);

    UA_CHECK(namespace_app_generated(server));
    UA_CHECK(UA_Server_run_startup(server));

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Step 1: server is ready");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "CTRL+C to stop");

#if workshopStep > 1
    //---------------------------------------------------------------
    // Step 2: search for devices availabe in the server's DeviceSet
    // DeviceSet is defined by OPC UA Device Integration and represents the collection of devices in a server
    //---------------------------------------------------------------

#endif

    runServer(server);

    return EXIT_SUCCESS;
}
