# ESP-IDF project file for the applet-device (WASM runtime) example.
#
# Build:
#   cd examples/applet-device/esp-idf
#   idf.py set-target esp32c6
#   idf.py build
#   idf.py flash monitor

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../tw-device-sdk"
)

set(PARTITION_TABLE_CUSTOM_FILENAME
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../partitions_applet.csv")
set(PARTITION_TABLE_OFFSET 0x8000)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(applet_device)
