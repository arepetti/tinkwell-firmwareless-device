# PAL Reference

The Platform Abstraction Layer (PAL) is the boundary between the SDK and the underlying hardware/OS.
All platform-specific code lives below this line.

## Writing a new backend

To port the SDK to a new platform (e.g. Zephyr, Fuchsia):

1. Create a directory: `pal/<platform>/src/`
2. Implement every function declared in the `pal/include/` headers.
3. Add a CMakeLists.txt or component registration file.
4. Set `PAL_BACKEND=<platform>` when building.

See [Porting to other boards](../guides/porting.md) for full examples with STM32, nRF52840, and RP2040.

## Headers

### pal_log.h

```c
void pal_log(pal_log_level_t level, const char *tag, const char *fmt, ...);
```

Convenience macros: `PAL_LOGE`, `PAL_LOGW`, `PAL_LOGI`, `PAL_LOGD`, `PAL_LOGV`.

### pal_gpio.h

```c
tw_err_t pal_gpio_init(int pin, pal_gpio_mode_t mode);
tw_err_t pal_gpio_write(int pin, bool level);
bool     pal_gpio_read(int pin);
tw_err_t pal_gpio_set_interrupt(int pin, pal_gpio_edge_t edge,
                                pal_gpio_isr_t handler, void *ctx);
```

Modes: `INPUT`, `OUTPUT`, `INPUT_PULLUP`, `INPUT_PULLDOWN`.
Edges: `RISING`, `FALLING`, `BOTH`.

### pal_i2c.h

```c
tw_err_t pal_i2c_init(const pal_i2c_config_t *cfg);
tw_err_t pal_i2c_read(int bus, uint8_t addr, uint8_t reg,
                      uint8_t *buf, size_t len);
tw_err_t pal_i2c_write(int bus, uint8_t addr, uint8_t reg,
                       const uint8_t *buf, size_t len);
void     pal_i2c_deinit(int bus);
```

### pal_net.h

```c
tw_err_t     pal_net_init(void);
pal_socket_t pal_udp_open(uint16_t local_port);
int          pal_udp_sendto(pal_socket_t s, const void *buf, size_t len,
                            const pal_addr_t *dest);
int          pal_udp_recvfrom(pal_socket_t s, void *buf, size_t len,
                              pal_addr_t *src, int timeout_ms);
void         pal_socket_close(pal_socket_t s);
void         pal_net_deinit(void);
```

### pal_os.h

```c
tw_err_t pal_mutex_create(pal_mutex_t *out);
tw_err_t pal_mutex_lock(pal_mutex_t m);
tw_err_t pal_mutex_unlock(pal_mutex_t m);
void     pal_mutex_destroy(pal_mutex_t m);

tw_err_t pal_sem_create(pal_sem_t *out, unsigned int initial);
tw_err_t pal_sem_wait(pal_sem_t s, int timeout_ms);
tw_err_t pal_sem_post(pal_sem_t s);
void     pal_sem_destroy(pal_sem_t s);

tw_err_t pal_task_create(const char *name, pal_task_fn_t fn, void *arg,
                         size_t stack_size, int priority);
void     pal_sleep_ms(uint32_t ms);
uint64_t pal_uptime_ms(void);
```

### pal_nvs.h

Non-volatile key-value storage.

```c
tw_err_t pal_nvs_init(void);
tw_err_t pal_nvs_get_i32 / set_i32(const char *key, ...);
tw_err_t pal_nvs_get_str / set_str(const char *key, ...);
tw_err_t pal_nvs_get_blob / set_blob(const char *key, ...);
tw_err_t pal_nvs_erase(const char *key);
tw_err_t pal_nvs_commit(void);
```

### pal_power.h

```c
tw_err_t          pal_power_deep_sleep(uint32_t duration_ms);
pal_wake_reason_t pal_power_wake_reason(void);
tw_err_t          pal_power_set_wake_gpio(int pin, bool level);
```

### pal_ota.h

```c
tw_err_t pal_ota_begin(pal_ota_handle_t *out, size_t image_size);
tw_err_t pal_ota_write(pal_ota_handle_t h, const void *data, size_t len);
tw_err_t pal_ota_finish(pal_ota_handle_t h);
tw_err_t pal_ota_abort(pal_ota_handle_t h);
tw_err_t pal_ota_set_boot_partition(void);
tw_err_t pal_ota_mark_valid(void);
tw_err_t pal_ota_rollback(void);
bool     pal_ota_is_pending_verify(void);
```

### pal_system.h

```c
void              pal_system_reboot(void);
pal_boot_reason_t pal_system_boot_reason(void);
uint32_t          pal_system_free_heap(void);
const char       *pal_system_chip_info(void);
```

### pal_flash.h

```c
tw_err_t    pal_flash_init(const char *label);
tw_err_t    pal_flash_erase(const char *label);
tw_err_t    pal_flash_write(const char *label, size_t offset,
                            const void *data, size_t len);
tw_err_t    pal_flash_read(const char *label, size_t offset,
                           void *buf, size_t len);
size_t      pal_flash_size(const char *label);
const void *pal_flash_mmap(const char *label, size_t *out_len);
void        pal_flash_munmap(const char *label);
```

## Existing backends

| Backend | Directory | Notes |
|---------|-----------|-------|
| POSIX | `pal/posix/` | Linux, macOS, WSL. Sockets, pthreads, file-based NVS. |
| ESP-IDF | `pal/esp-idf/` | FreeRTOS, lwIP, NVS flash, partition-based OTA/flash. |
| Mock | `pal/mock/` | Unit testing. Records calls, returns injectable values. |
