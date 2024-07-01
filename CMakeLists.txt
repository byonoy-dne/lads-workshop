cmake_minimum_required(VERSION 3.14)

project(lads-workshop-server LANGUAGES C)

add_executable(lads-workshop-server
    src/main.c
)

include(ExternalProject)

set(PROJECT_NODESETS_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/workshop/nodesets")
set(PROJECT_NS0_FILE ${PROJECT_NODESETS_ROOT}/Opc.Ua.NodeSet2.xml)
set(PROJECT_NODESETS
    ${PROJECT_NODESETS_ROOT}/Opc.Ua.DI.NodeSet2.xml
    ${PROJECT_NODESETS_ROOT}/Opc.Ua.AMB.NodeSet2.xml
    ${PROJECT_NODESETS_ROOT}/Opc.Ua.Machinery.NodeSet2.xml
    ${PROJECT_NODESETS_ROOT}/Opc.Ua.LADS.NodeSet2.xml
    ${PROJECT_NODESETS_ROOT}/LuminescenceReader.xml
)
set(OPEN62541_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/open62541")
set(OPEN62541_BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/open62541-build")

set(open62541_OPTIONS
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DUA_ENABLE_AMALGAMATION=ON"
    "-DUA_NAMESPACE_ZERO=FULL"
    "-DUA_LOGLEVEL=300"
    "-DUA_FILE_NS0=${PROJECT_NS0_FILE}"
)

ExternalProject_Add(open62541_build
    BINARY_DIR ${OPEN62541_BINARY_DIR}
    SOURCE_DIR ${OPEN62541_SOURCE_DIR}
    CMAKE_ARGS  ${open62541_OPTIONS}
    BUILD_COMMAND cmake --build . --target open62541-code-generation
    INSTALL_COMMAND ""
)

set(open62541_TOOLS_DIR ${OPEN62541_SOURCE_DIR}/tools)
include(${open62541_TOOLS_DIR}/cmake/macros_public.cmake)
# The above path changes in 1.4.1:
# include(${open62541_TOOLS_DIR}/cmake/open62541Macros.cmake)

ua_generate_nodeset(
    NAME app
    FILE "${PROJECT_NODESETS}"
    DEPENDS_NS "${PROJECT_NS0_FILE}"
    OUTPUT_DIR ${OPEN62541_BINARY_DIR}
)

add_dependencies(open62541-generator-ns-app open62541_build)
add_dependencies(lads-workshop-server open62541-generator-ns-app)
add_dependencies(lads-workshop-server open62541_build)

set_source_files_properties(
    ${OPEN62541_BINARY_DIR}/namespace_app_generated.c
    ${OPEN62541_BINARY_DIR}/open62541.c
    PROPERTIES
    COMPILE_FLAGS -Wno-maybe-uninitialized
    GENERATED 1
)

target_sources(lads-workshop-server PRIVATE
    ${OPEN62541_BINARY_DIR}/namespace_app_generated.c
    ${OPEN62541_BINARY_DIR}/open62541.c
)

target_include_directories(lads-workshop-server PUBLIC ${OPEN62541_BINARY_DIR})
target_compile_definitions(lads-workshop-server PUBLIC UA_ENABLE_AMALGAMATION)

install(TARGETS lads-workshop-server)
