cmake_minimum_required(VERSION 3.16)
project(applet_device C)

if(NOT PAL_BACKEND)
    set(PAL_BACKEND "posix")
endif()

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tw-device-sdk
    ${CMAKE_CURRENT_BINARY_DIR}/tw-device-sdk
)

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/src/wasm_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/src/wasm_host_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/host/main.c
)

add_executable(applet_device ${APP_SOURCES})

target_include_directories(applet_device PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
)

target_link_libraries(applet_device PRIVATE tw_device_sdk)
target_compile_options(applet_device PRIVATE -Wall -Wextra -Wpedantic -std=c11)
