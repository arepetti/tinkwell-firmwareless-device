# ESP-IDF component registration for the PAL backend.
# This file is included by the SDK CMakeLists.txt when building under ESP-IDF.

idf_component_register(
    SRCS
        "src/pal_log_esp.c"
        "src/pal_gpio_esp.c"
        "src/pal_i2c_esp.c"
        "src/pal_net_esp.c"
        "src/pal_os_freertos.c"
        "src/pal_nvs_esp.c"
        "src/pal_power_esp.c"
        "src/pal_ota_esp.c"
        "src/pal_system_esp.c"
        "src/pal_flash_esp.c"
    INCLUDE_DIRS
        "../include"
        "../../include"
        "../../transport/include"
    REQUIRES
        driver
        esp_wifi
        esp_netif
        nvs_flash
        esp_partition
        esp_ota_ops
        esp_timer
        log
        lwip
)
