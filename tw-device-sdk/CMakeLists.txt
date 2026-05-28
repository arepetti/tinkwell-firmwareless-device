cmake_minimum_required(VERSION 3.16)

# tw-device-sdk
#
# Dual-mode build file:
#   1) ESP-IDF component   -- detected by the presence of IDF_TARGET.
#   2) Standalone CMake lib -- included via add_subdirectory() for POSIX builds.

set(TW_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Nanopb-generated protobuf sources (populated after code generation below).
set(TW_PROTO_DIR ${TW_SDK_DIR}/proto)
set(TW_PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto_generated)

# Common (platform-independent) SDK sources.
set(TW_SDK_CORE_SOURCES
    ${TW_SDK_DIR}/svc/src/svc_device.c
    ${TW_SDK_DIR}/svc/src/svc_sensor.c
    ${TW_SDK_DIR}/svc/src/svc_config.c
    ${TW_SDK_DIR}/svc/src/svc_heartbeat.c
    ${TW_SDK_DIR}/svc/src/svc_ota.c
    ${TW_SDK_DIR}/svc/src/svc_applet.c
    ${TW_SDK_DIR}/svc/src/svc_power.c
    ${TW_SDK_DIR}/svc/src/svc_provision.c
    ${TW_SDK_DIR}/svc/src/svc_identity.c
    ${TW_SDK_DIR}/svc/src/svc_cmd.c
    ${TW_SDK_DIR}/svc/src/svc_reset_button.c
    ${TW_SDK_DIR}/svc/src/svc_telemetry.c
    ${TW_SDK_DIR}/protocol/src/proto_coap.c
    ${TW_SDK_DIR}/protocol/src/proto_mqtt.c
    ${TW_SDK_DIR}/util/src/tw_button.c
    ${TW_SDK_DIR}/util/src/tw_led.c
    ${TW_SDK_DIR}/util/src/tw_safety.c
    ${TW_SDK_DIR}/util/src/tw_kvtext.c
    ${TW_SDK_DIR}/util/src/tw_block.c
)

set(TW_SDK_INCLUDE_DIRS
    ${TW_SDK_DIR}/include
    ${TW_SDK_DIR}/pal/include
    ${TW_SDK_DIR}/transport/include
    ${TW_PROTO_GENERATED_DIR}
)

# ---- Mode 1: ESP-IDF component ----
if(IDF_TARGET)
    set(TW_PAL_ESP_SOURCES
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_log_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_gpio_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_i2c_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_net_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_os_freertos.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_nvs_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_power_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_ota_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_system_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_flash_esp.c
        ${TW_SDK_DIR}/pal/esp-idf/src/pal_crypto_esp.c
        ${TW_SDK_DIR}/transport/src/transport_wifi.c
        ${TW_SDK_DIR}/transport/src/transport_eth.c
        ${TW_SDK_DIR}/transport/src/transport_thread.c
        ${TW_SDK_DIR}/transport/src/transport_ble.c
    )

    # nanopb is available via the ESP-IDF component registry (espressif/nanopb).
    # Code generation: the nanopb CMake integration provides nanopb_generate_cpp().
    # We use a custom command that works with or without the CMake helper.
    set(TW_PROTO_SRC ${TW_PROTO_GENERATED_DIR}/tw_protocol.pb.c)
    set(TW_PROTO_HDR ${TW_PROTO_GENERATED_DIR}/tw_protocol.pb.h)

    idf_component_register(
        SRCS
            ${TW_SDK_CORE_SOURCES}
            ${TW_PAL_ESP_SOURCES}
            ${TW_PROTO_SRC}
        INCLUDE_DIRS
            ${TW_SDK_INCLUDE_DIRS}
        REQUIRES
            driver
            esp_wifi
            esp_netif
            esp_event
            nvs_flash
            esp_partition
            esp_ota_ops
            esp_timer
            esp_system
            log
            lwip
            freertos
            coap
            nanopb
    )

    target_compile_definitions(${COMPONENT_LIB} PUBLIC TW_PLATFORM_ESPIDF=1)

    # Generate protobuf C sources using the nanopb protoc plugin.
    # The nanopb ESP-IDF component provides the generator at its tools/ path.
    file(MAKE_DIRECTORY ${TW_PROTO_GENERATED_DIR})
    idf_build_get_property(NANOPB_DIR nanopb COMPONENT_DIR)
    add_custom_command(
        OUTPUT ${TW_PROTO_SRC} ${TW_PROTO_HDR}
        COMMAND protoc
            --plugin=protoc-gen-nanopb=${NANOPB_DIR}/generator/protoc-gen-nanopb
            --nanopb_out=${TW_PROTO_GENERATED_DIR}
            -I${TW_PROTO_DIR}
            ${TW_PROTO_DIR}/tw_protocol.proto
        DEPENDS
            ${TW_PROTO_DIR}/tw_protocol.proto
            ${TW_PROTO_DIR}/tw_protocol.options
        COMMENT "Generating nanopb sources from tw_protocol.proto"
    )

# ---- Mode 2: Standalone POSIX build ----
else()
    # Pull libcoap via FetchContent for binary CoAP (RFC 7252) support.
    include(FetchContent)
    FetchContent_Declare(
        libcoap
        GIT_REPOSITORY https://github.com/obgm/libcoap.git
        GIT_TAG        v4.3.5
    )
    set(ENABLE_DTLS OFF CACHE BOOL "" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(ENABLE_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libcoap)

    # Pull nanopb for protobuf support.
    FetchContent_Declare(
        nanopb
        GIT_REPOSITORY https://github.com/nanopb/nanopb.git
        GIT_TAG        0.4.9.1
    )
    set(NANOPB_SRC_ROOT_FOLDER ${nanopb_SOURCE_DIR} CACHE INTERNAL "")
    FetchContent_MakeAvailable(nanopb)

    # Generate protobuf C sources using nanopb.
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
            COMMENT "Generating nanopb sources from tw_protocol.proto (POSIX)"
        )
    else()
        message(WARNING "protoc not found -- using pre-generated nanopb sources if available")
    endif()

    set(TW_PAL_POSIX_SOURCES
        ${TW_SDK_DIR}/pal/posix/src/pal_log_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_gpio_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_i2c_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_net_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_os_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_nvs_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_power_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_ota_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_system_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_flash_posix.c
        ${TW_SDK_DIR}/pal/posix/src/pal_crypto_posix.c
        ${TW_SDK_DIR}/transport/src/transport_posix.c
    )

    add_library(tw_device_sdk STATIC
        ${TW_SDK_CORE_SOURCES}
        ${TW_PAL_POSIX_SOURCES}
        ${TW_PROTO_GENERATED_DIR}/tw_protocol.pb.c
    )

    target_include_directories(tw_device_sdk PUBLIC
        ${TW_SDK_INCLUDE_DIRS}
        ${nanopb_SOURCE_DIR}
    )
    target_link_libraries(tw_device_sdk PUBLIC pthread coap-3 protobuf-nanopb)

    target_compile_options(tw_device_sdk PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -std=c11
    )
endif()
