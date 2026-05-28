# Native Development (POSIX Build)

The POSIX backend lets you compile and run device firmware as a regular Linux/macOS executable.
This is the primary development workflow -- fast builds, easy debugging, no hardware needed.

## How it works

The POSIX PAL implements every abstraction using standard system calls:

| PAL header | POSIX implementation |
|------------|---------------------|
| `pal_gpio` | In-memory pin array (simulated) |
| `pal_i2c` | Fake sensor data from `TW_FAKE_TEMP` / `TW_FAKE_HUMID` env vars |
| `pal_net` | BSD sockets (`socket`, `sendto`, `recvfrom`) |
| `pal_os` | pthreads, POSIX semaphores, `clock_gettime` |
| `pal_nvs` | Flat file at `$HOME/.tw-device/nvs.dat` |
| `pal_power` | Simulated with `usleep` (logs a message) |
| `pal_ota` | File at `$HOME/.tw-device/ota_staging.bin` |
| `pal_flash` | Files at `$HOME/.tw-device/<label>.bin`, `mmap` for XIP |
| `pal_system` | `exit(0)` for reboot, returns `"posix-host"` |
| `pal_log` | `fprintf(stderr)` with timestamps |

## Building

```bash
cd examples/thermostat
cmake -B build -DPAL_BACKEND=posix
cmake --build build
```

## Running

```bash
./build/thermostat
```

The CoAP server listens on UDP port 5683.
Query with `tw`:

```bash
tw coap send get /tw/sensor/temperature
```

## Simulating sensors

```bash
export TW_FAKE_TEMP=215    # 21.5 C
export TW_FAKE_HUMID=450   # 45.0 %RH
./build/thermostat
```

## Simulating button presses

The POSIX GPIO backend provides `pal_gpio_sim_set(pin, level)`.
Integration tests can call this via a Unix socket or inject it from a test harness.

## Debugging

Standard tools work: `gdb`, `valgrind`, `strace`, address sanitizer.

```bash
cmake -B build-debug -DPAL_BACKEND=posix -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
gdb ./build-debug/thermostat
```

## NVS persistence

Config values are stored in `$HOME/.tw-device/nvs.dat`.
Delete it to start fresh:

```bash
rm -rf ~/.tw-device
```

## Related documentation

- [Quick start](../getting-started/quick-start.md) -- build and run in 5 minutes
- [ESP-IDF guide](esp-idf.md) -- when you are ready for real hardware
- [QEMU testing](qemu-testing.md) -- emulated ESP32 without a board
