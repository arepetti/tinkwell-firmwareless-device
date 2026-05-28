# main component for the applet-device ESP-IDF project.

idf_component_register(
    SRCS
        "app_main.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wasm_runtime.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wasm_host_api.c"
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/include"
    REQUIRES
        tw-device-sdk
)
