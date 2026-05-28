# main component for the thermostat ESP-IDF project.

idf_component_register(
    SRCS
        "app_main.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/thermostat.c"
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../app/include"
    REQUIRES
        tw-device-sdk
)
