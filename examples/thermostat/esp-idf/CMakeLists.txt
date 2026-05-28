# ESP-IDF project file for the thermostat example.
#
# Build:
#   cd examples/thermostat/esp-idf
#   idf.py set-target esp32c6      # or esp32c3 for QEMU
#   idf.py menuconfig               # optional: set WiFi SSID/password
#   idf.py build
#   idf.py flash monitor

cmake_minimum_required(VERSION 3.16)

# Register the SDK root as an extra component directory so the
# ESP-IDF build system discovers it via idf_component_register().
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../tw-device-sdk"
)

# Custom partition table with A/B OTA slots.
set(PARTITION_TABLE_CUSTOM_FILENAME
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../partitions_ota.csv")
set(PARTITION_TABLE_OFFSET 0x8000)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermostat)
