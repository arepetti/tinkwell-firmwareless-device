cmake_minimum_required(VERSION 3.16)
project(thermostat C)

# PAL_BACKEND selects the platform: "posix" (default) or "esp-idf".
if(NOT PAL_BACKEND)
    set(PAL_BACKEND "posix")
endif()

# Include the SDK.
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tw-device-sdk
    ${CMAKE_CURRENT_BINARY_DIR}/tw-device-sdk
)

# Application sources.
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/thermostat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/host/main.c
)

add_executable(thermostat ${APP_SOURCES})

target_include_directories(thermostat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app/include
)

target_link_libraries(thermostat PRIVATE tw_device_sdk)

target_compile_options(thermostat PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -std=c11
)
