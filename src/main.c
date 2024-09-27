#include "namespace_app_generated.h"
#include "open62541.h"

#include <pthread.h>
#include <signal.h>

#define WORKSHOP_STEP 13

#define WORKER_UPDATE_RATE_US 500000
#define DAMPING 0.8
#define WELLS 96

#define UA_CHECK(status)                                                                                               \
    do {                                                                                                               \
        if (status != UA_STATUSCODE_GOOD) {                                                                            \
            UA_Server_delete(server);                                                                                  \
            return EXIT_FAILURE;                                                                                       \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)

static volatile UA_Boolean running = true;
pthread_t sensorThread = 0;

typedef struct
{
    UA_NodeId id;
    UA_NodeId currentValue;
    UA_NodeId targetValue;
    UA_NodeId isEnabled;
} analog_control_function_t;

struct
{
    UA_NodeId cover;
    UA_NodeId injector1;
    UA_NodeId injector2;
    UA_NodeId injector3;
    UA_NodeId luminescenceSensor;
    UA_NodeId shakerController;
    analog_control_function_t temperatureController;
    UA_NodeId wastePump;
} functionSet;

struct
{
    UA_Double targetTemperature;
    UA_Double currentTemperature;
    UA_Boolean temperatureControllerIsOn;
} deviceState;

static void stopHandler(int sig)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received CTRL+C");
    running = false;
}

UA_Double randomDouble()
{
    return ((UA_Double)rand()) / RAND_MAX;
}

// Step 4
void updateSensors(UA_Server* server)
{
    // temperature with 1st order low pass filter and noise
    UA_Double temperature = deviceState.temperatureControllerIsOn ? deviceState.targetTemperature : 25;
    deviceState.currentTemperature = DAMPING * deviceState.currentTemperature + (1 - DAMPING) * temperature;
    UA_Double currentTemperatureWithNoise = deviceState.currentTemperature + 0.2 * (randomDouble() - 0.5);

    // array of luminescence readings with some noise
    UA_Double luminescenceWithNoise[WELLS];
    for (int i = 0; i < WELLS; ++i) {
        luminescenceWithNoise[i] = i * i + (randomDouble() - 0.5);
    }

    // use the setValueFromSource() function to update the variables in the OPC UA information model
    UA_Variant temperatureVariant, luminescenceVariant;
    UA_Variant_setScalar(&temperatureVariant, &currentTemperatureWithNoise, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, functionSet.temperatureController.currentValue, temperatureVariant);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Temperature: %f", currentTemperatureWithNoise);
}

void* worker(void* server)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Background worker started");

    while (running) {
        updateSensors(server);
        usleep(WORKER_UPDATE_RATE_US);
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Background worker shutting down");

    return NULL;
}

UA_StatusCode runServer(UA_Server* server)
{
    while (running) {
        UA_Server_run_iterate(server, true);
    }

    if (sensorThread) {
        pthread_join(sensorThread, NULL);
    }

    UA_StatusCode result = UA_Server_run_shutdown(server);

    return result;
}

int main(int argc, char* argv[])
{
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    srand(time(NULL));

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
#if WORKSHOP_STEP >= 2
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

    // stop here if we only want to show step 2
#if WORKSHOP_STEP >= 3
    //---------------------------------------------------------------
    // Step 3: access the device as LuminescenseReader
    //---------------------------------------------------------------
    // The TS workshop project does some wonderful high-level language trickery to wrap the device representation and
    // access. We can't really do that in C, so here we'll settle for defining all the node IDs we're going to use.

    // We could browse the tree as demonstrated in step 2 to dynamically find the nodes, but in a resource constrained
    // environment it is probably best to just hard-code the node IDs. You could also use the CMake macro
    // ua_generate_nodeid_header to generate named constants at compile time, but that requires an additional CSV file
    // per nodeset. The official nodeset repo at https://github.com/OPCFoundation/UA-Nodeset contains these files for
    // all namespaces, including LADS. But the workshop project does not provide a CSV file for the
    // LuminescenceReader.xml, so we don't use that mechanism here.

    size_t namespaceLumiIndex;
    UA_CHECK(
      UA_Server_getNamespaceByName(server, UA_STRING("http://spectaris.de/LuminescenceReader/"), &namespaceLumiIndex));
    functionSet.cover = UA_NODEID_NUMERIC(namespaceLumiIndex, 5049);
    functionSet.injector1 = UA_NODEID_NUMERIC(namespaceLumiIndex, 5051);
    functionSet.injector2 = UA_NODEID_NUMERIC(namespaceLumiIndex, 5052);
    functionSet.injector3 = UA_NODEID_NUMERIC(namespaceLumiIndex, 5053);
    functionSet.luminescenceSensor = UA_NODEID_NUMERIC(namespaceLumiIndex, 5054);
    functionSet.shakerController = UA_NODEID_NUMERIC(namespaceLumiIndex, 5055);
    functionSet.temperatureController.id = UA_NODEID_NUMERIC(namespaceLumiIndex, 5054);
    functionSet.temperatureController.currentValue = UA_NODEID_NUMERIC(namespaceLumiIndex, 6177);
    functionSet.temperatureController.targetValue = UA_NODEID_NUMERIC(namespaceLumiIndex, 6178);
    functionSet.temperatureController.isEnabled = UA_NODEID_NUMERIC(namespaceLumiIndex, 6179);
    functionSet.wastePump = UA_NODEID_NUMERIC(namespaceLumiIndex, 5054);
#endif

    // stop here if we only want to show step 3
#if WORKSHOP_STEP >= 4
    //---------------------------------------------------------------
    // Step 4: set OPC UA variable values from internal variables (use case: read values from the device)
    //---------------------------------------------------------------

    deviceState.targetTemperature = 37.0;
    deviceState.currentTemperature = 25.0;
    deviceState.temperatureControllerIsOn = UA_TRUE;

    // The TS workshop project put the logic into a lambda. We don't have that luxury so all of this step's logic is in
    // the updateSensors funtion.
    pthread_create(&sensorThread, NULL, worker, server);
#endif

    UA_CHECK(runServer(server));
    UA_Server_delete(server);

    return EXIT_SUCCESS;
}
