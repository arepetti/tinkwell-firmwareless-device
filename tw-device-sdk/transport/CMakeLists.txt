# Transport source registration for ESP-IDF component build.

if(IDF_TARGET)
    idf_component_register(
        SRCS
            "src/transport_wifi.c"
        INCLUDE_DIRS
            "include"
            "../include"
            "../pal/include"
        REQUIRES
            esp_wifi
            esp_netif
            esp_event
            nvs_flash
            log
    )
else()
    # For the POSIX CMake build, the transport_posix.c is compiled
    # as part of the main SDK library target.
endif()
