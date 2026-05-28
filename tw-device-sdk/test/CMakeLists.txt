cmake_minimum_required(VERSION 3.16)
project(tw_device_sdk_tests C)

enable_testing()

set(SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# -- Dependencies via FetchContent --

include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

FetchContent_Declare(
    nanopb
    GIT_REPOSITORY https://github.com/nanopb/nanopb.git
    GIT_TAG        0.4.9.1
)
FetchContent_MakeAvailable(nanopb)

# -- Protobuf code generation --

set(TW_PROTO_DIR ${SDK_DIR}/proto)
set(TW_PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto_generated)
file(MAKE_DIRECTORY ${TW_PROTO_GENERATED_DIR})

find_program(PROTOC protoc)
if(PROTOC)
    add_custom_command(
        OUTPUT
            ${TW_PROTO_GENERATED_DIR}/tw_protocol.pb.c
            ${TW_PROTO_GENERATED_DIR}/tw_protocol.pb.h
        COMMAND ${PROTOC}
            --plugin=protoc-gen-nanopb=${nanopb_SOURCE_DIR}/generator/protoc-gen-nanopb
            --nanopb_out=${TW_PROTO_GENERATED_DIR}
            -I${TW_PROTO_DIR}
            ${TW_PROTO_DIR}/tw_protocol.proto
        DEPENDS
            ${TW_PROTO_DIR}/tw_protocol.proto
            ${TW_PROTO_DIR}/tw_protocol.options
        COMMENT "Generating nanopb sources from tw_protocol.proto (tests)"
    )
else()
    message(FATAL_ERROR "protoc is required for test build")
endif()

# -- Compile flags --

set(COMMON_C_FLAGS -std=c11 -Wall -Wextra -Wpedantic)

# Enable protobuf path in SDK sources under test.
add_compile_definitions(
    CONFIG_TW_USE_PROTOBUF=1
    CONFIG_TW_THREAD_SAFETY=0
)

# -- Common includes --

set(SDK_INCLUDE_DIRS
    ${SDK_DIR}/include
    ${SDK_DIR}/pal/include
    ${SDK_DIR}/transport/include
    ${SDK_DIR}/pal/mock/include
    ${TW_PROTO_GENERATED_DIR}
    ${nanopb_SOURCE_DIR}
)

# -- Mock PAL sources --

file(GLOB MOCK_PAL_SOURCES ${SDK_DIR}/pal/mock/src/*.c)

# -- Shared SDK utility sources needed by tests --

set(SDK_UTILITY_SOURCES
    ${SDK_DIR}/svc/src/svc_config.c
    ${SDK_DIR}/util/src/tw_kvtext.c
    ${TEST_DIR}/support/tw_msg_helpers.c
    ${TW_PROTO_GENERATED_DIR}/tw_protocol.pb.c
)

# -- Common test support library --

add_library(test_support STATIC
    ${MOCK_PAL_SOURCES}
    ${SDK_UTILITY_SOURCES}
)

target_include_directories(test_support PUBLIC ${SDK_INCLUDE_DIRS})
target_compile_options(test_support PRIVATE ${COMMON_C_FLAGS})
target_link_libraries(test_support PUBLIC protobuf-nanopb)

# -- Helper function to add a test executable --

function(add_sdk_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_include_directories(${TEST_NAME} PRIVATE
        ${SDK_INCLUDE_DIRS}
        ${unity_SOURCE_DIR}/src
    )
    target_link_libraries(${TEST_NAME} PRIVATE
        test_support
        unity
        protobuf-nanopb
    )
    target_compile_options(${TEST_NAME} PRIVATE ${COMMON_C_FLAGS})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# ── New protobuf test executables ───────────────────────────────────

add_sdk_test(test_proto_roundtrip
    ${TEST_DIR}/unit/test_proto_roundtrip.c
)

add_sdk_test(test_cmd_handlers
    ${TEST_DIR}/unit/test_cmd_handlers.c
    ${SDK_DIR}/svc/src/svc_cmd.c
)

add_sdk_test(test_provision_proto
    ${TEST_DIR}/unit/test_provision_proto.c
    ${SDK_DIR}/svc/src/svc_provision.c
    ${SDK_DIR}/svc/src/svc_identity.c
    ${SDK_DIR}/svc/src/svc_heartbeat.c
)

# ── Pre-existing test executables ──────────────────────────────────

add_sdk_test(test_kvtext
    ${TEST_DIR}/unit/test_kvtext.c
)

add_sdk_test(test_coap_dispatch
    ${TEST_DIR}/unit/test_coap_dispatch.c
)

add_sdk_test(test_identity
    ${TEST_DIR}/unit/test_identity.c
    ${SDK_DIR}/svc/src/svc_identity.c
)

add_sdk_test(test_heartbeat
    ${TEST_DIR}/unit/test_heartbeat.c
    ${SDK_DIR}/svc/src/svc_identity.c
    ${SDK_DIR}/svc/src/svc_heartbeat.c
)

add_sdk_test(test_button_debounce
    ${TEST_DIR}/unit/test_button_debounce.c
    ${SDK_DIR}/util/src/tw_button.c
)

add_sdk_test(test_safety_monitor
    ${TEST_DIR}/unit/test_safety_monitor.c
    ${SDK_DIR}/util/src/tw_safety.c
)

add_sdk_test(test_ota_state
    ${TEST_DIR}/unit/test_ota_state.c
    ${SDK_DIR}/svc/src/svc_ota.c
)

add_sdk_test(test_applet_lifecycle
    ${TEST_DIR}/unit/test_applet_lifecycle.c
    ${SDK_DIR}/svc/src/svc_applet.c
)
