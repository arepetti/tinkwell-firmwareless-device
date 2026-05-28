# Porting to Other Boards

The TW Device SDK is designed around a Platform Abstraction Layer (PAL) that cleanly separates hardware-specific code from application logic and SDK services.
Porting to an ARM Cortex-M board (or any other target) is a well-scoped task that involves implementing the PAL for the new platform without touching the SDK core or application code.

## Scope of a port

The following PAL modules need a board-specific implementation:

| Module            | Header            | Typical MCU peripheral          |
|-------------------|-------------------|---------------------------------|
| `pal_gpio`        | `pal_gpio.h`      | GPIO registers                  |
| `pal_i2c`         | `pal_i2c.h`       | I2C / TWI peripheral            |
| `pal_nvs`         | `pal_nvs.h`       | Internal flash or EEPROM        |
| `pal_net`         | `pal_net.h`       | Ethernet MAC / WiFi driver      |
| `pal_os`          | `pal_os.h`        | RTOS primitives (tasks, mutexes)|
| `pal_log`         | `pal_log.h`       | UART / SWO / RTT                |
| `pal_system`      | `pal_system.h`    | Reset, chip info, heap stats    |
| `pal_power`       | `pal_power.h`     | Sleep modes, wake sources       |
| `pal_ota`         | `pal_ota.h`       | Dual-bank flash / bootloader    |
| `pal_flash`       | `pal_flash.h`     | Flash partitions (applet store) |

Each PAL module is a thin C file that maps the SDK's abstract API to the vendor HAL.

## Example: STM32L4 (Cortex-M4) with FreeRTOS

```
tw-device-sdk/
└── pal/
    └── stm32l4/
        └── src/
            ├── pal_gpio_stm32.c
            ├── pal_i2c_stm32.c
            ├── pal_nvs_stm32.c      (using internal flash pages)
            ├── pal_net_stm32.c       (lwIP + ETH or WiFi module)
            ├── pal_os_freertos.c     (shared with ESP-IDF port)
            ├── pal_log_stm32.c       (UART or SWO)
            ├── pal_system_stm32.c
            ├── pal_power_stm32.c     (STOP2 / STANDBY modes)
            ├── pal_ota_stm32.c       (dual-bank boot)
            └── pal_flash_stm32.c
```

### CMake integration

Add a new `TW_PAL_STM32_SOURCES` list in the SDK's `CMakeLists.txt`, gated by a `TW_PAL_BACKEND` variable:

```cmake
if(TW_PAL_BACKEND STREQUAL "stm32l4")
    set(TW_PAL_SOURCES
        ${TW_SDK_DIR}/pal/stm32l4/src/pal_gpio_stm32.c
        ${TW_SDK_DIR}/pal/stm32l4/src/pal_i2c_stm32.c
        # ... etc
    )
    target_link_libraries(tw_device_sdk PUBLIC
        stm32l4xx_hal   # vendor HAL
        freertos_kernel  # FreeRTOS
    )
endif()
```

### Transport

For WiFi-equipped boards (e.g. STM32 + ESP-AT co-processor, or STM32WB with BLE), implement the appropriate transport in `transport/src/transport_<name>.c`.
For Ethernet-only boards, the existing `transport_eth.c` structure can be reused with a board-specific Ethernet driver.

Thread (802.15.4) requires an OpenThread port for the target radio, which is available for several STM32WB and nRF52/nRF53 platforms.

## Example: nRF52840 (Cortex-M4F) with Zephyr

Zephyr's device model is well-suited for the PAL pattern.
Each PAL module wraps Zephyr's device tree APIs:

- `pal_gpio` -> `gpio_pin_configure()` / `gpio_pin_set()`
- `pal_i2c` -> `i2c_write()` / `i2c_read()`
- `pal_nvs` -> Zephyr NVS subsystem
- `pal_net` -> Zephyr's BSD sockets or `net_context`
- `pal_os` -> `k_thread_create()`, `k_mutex_lock()`

The nRF52840 natively supports BLE and Thread (via OpenThread), making it an excellent target for the SDK's multi-transport architecture.

## Example: RP2040 (Cortex-M0+)

The RP2040 has limited flash and RAM (264 KB SRAM, 2+ MB external flash).
A port is feasible but requires care:

- Use the Pico SDK's HAL for GPIO, I2C, SPI, flash
- `pal_os` can use FreeRTOS on the RP2040 or the Pico SDK's cooperative multitasking
- `pal_net` requires an external WiFi module (e.g. Pico W with CYW43439) or Ethernet (W5500 SPI)
- OTA requires a custom dual-bank flash layout in the external flash (the RP2040 boots from external QSPI flash)

## Key considerations

- **Flash budget**: The SDK core + CoAP stub is ~15 KB of code.
  With libcoap or a real MQTT client, budget ~30--50 KB.
- **RAM budget**: ~4 KB for SDK state + networking buffers.
  WASM applet runtime adds ~50 KB (stack + heap).
- **Crypto**: Ed25519 key derivation requires either mbedTLS (already available in ESP-IDF and Zephyr) or a lightweight Ed25519 library (~5 KB code).
- **Power**: Cortex-M deep sleep modes map directly to the `pal_power` API.
  Wake sources (RTC, GPIO) are board-specific but the interface is identical.

## Related documentation

- [PAL reference](../reference/pal.md) -- full PAL API specification
- [Architecture overview](../architecture/overview.md) -- layer diagram
- [Kconfig reference](../reference/kconfig.md) -- compile-time options
