cmake_minimum_required(VERSION 3.16)
project(minimal C)

if(NOT PAL_BACKEND)
    set(PAL_BACKEND "posix")
endif()

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tw-device-sdk
    ${CMAKE_CURRENT_BINARY_DIR}/tw-device-sdk
)

add_executable(minimal main.c)
target_link_libraries(minimal PRIVATE tw_device_sdk)
target_compile_options(minimal PRIVATE -Wall -Wextra -Wpedantic -std=c11)
