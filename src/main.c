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
    config->buildInfo.manufacturerName = UA_STRING_ALLOC("SPECTARIS");
    config->buildInfo.productUri = UA_STRING_ALLOC(uri);
    config->buildInfo.softwareVersion = UA_STRING_ALLOC("1.0.0");
    config->applicationDescription.applicationName = UA_LOCALIZEDTEXT_ALLOC("en", "LADS LuminescenceReader");
    config->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;
    config->applicationDescription.productUri = UA_STRING_ALLOC(uri);
    config->applicationDescription.applicationUri = UA_STRING_ALLOC(uri);

    UA_CHECK(namespace_app_generated(server));
    UA_CHECK(UA_Server_run_startup(server));

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Step 1: server is ready");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "CTRL+C to stop");

    // stop here if we only want to show step 1
#if workshopStep >= 2
    //---------------------------------------------------------------
    // Step 2: search for devices availabe in the server's DeviceSet
    // DeviceSet is defined by OPC UA Device Integration and represents the collection of devices in a server
    //---------------------------------------------------------------

    // The deviceSet Node instance is created by the DI Nodeset and is stable. Thats why we can use this ID
    // directly in its namespace to get directly to the node
    const UA_UInt32 deviceSetIdentifier = 5001;

    // To get the node instance we get a reference to the namespace it is located in and use the Node ID from above to
    // find it
    size_t namespaceDiIndex;
    UA_CHECK(UA_Server_getNamespaceByName(server, UA_STRING("http://opcfoundation.org/UA/DI/"), &namespaceDiIndex));
    const UA_NodeId deviceSetNodeID = UA_NODEID_NUMERIC(namespaceDiIndex, deviceSetIdentifier);
    const UA_NodeId referenceTypeAggregates = UA_NODEID_NUMERIC(0, 44);

    UA_BrowseDescription browseDescr;
    UA_BrowseDescription_init(&browseDescr);
    browseDescr.nodeId = deviceSetNodeID;
    browseDescr.browseDirection = UA_BROWSEDIRECTION_FORWARD;
    browseDescr.referenceTypeId = referenceTypeAggregates;
    browseDescr.includeSubtypes = UA_TRUE;
    browseDescr.resultMask = UA_BROWSERESULTMASK_BROWSENAME | UA_BROWSERESULTMASK_TYPEDEFINITION;

    UA_BrowseResult deviceBrowseResult = UA_Server_browse(server, UA_UINT32_MAX, &browseDescr);
    UA_ExpandedNodeId devices[deviceBrowseResult.referencesSize];
    for (size_t i = 0; i < deviceBrowseResult.referencesSize; ++i) {
        UA_ReferenceDescription* refDescr = deviceBrowseResult.references + i;
        devices[i] = refDescr->nodeId;

        UA_QualifiedName typeName;
        UA_QualifiedName_init(&typeName);
        UA_Server_readBrowseName(server, refDescr->typeDefinition.nodeId, &typeName);

        UA_LOG_INFO(UA_Log_Stdout,
                    UA_LOGCATEGORY_USERLAND,
                    "Step 2: Found device %.*s of type %.*s",
                    refDescr->browseName.name.length,
                    refDescr->browseName.name.data,
                    typeName.name.length,
                    typeName.name.data);
    }
    UA_BrowseResult_clear(&deviceBrowseResult);
#endif

    UA_CHECK(runServer(server));
    UA_Server_delete(server);

    return EXIT_SUCCESS;
}
