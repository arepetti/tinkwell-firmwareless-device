# Device scripts

Helper scripts bundled with the [Firmwareless device SDK](../README.md) live in this folder.
They automate common developer workflows on the host machine: flashing ESP-IDF builds, POSIX demo runs (`demo`), local CoAP tooling (`fake_hub`, `provision`), sensor simulation for the POSIX PAL (`fake_sensor`), and QEMU sessions (`qemu_run`).
Use these when iterating on device firmware or applets.
Production deployments use real hubs and hardware.
Going further, see [Build your own device](../docs/getting-started/build-your-device.md) and [Your first applet](../docs/getting-started/your-first-applet.md).

## Canonical working directory

Every synopsis and command example below assumes your shell **`cd`** matches the Firmwareless device package root **`extras/firmwareless/device/`** — the **`device/`** folder that siblings **`scripts/`**, **`examples/`**, **`docs/`**, and **`factory/`**.
Shell helpers are invoked as **`./scripts/<name>.*`** from there; Python CLIs use **`python3 scripts/<name>.py`** (substitute **`python`** on Windows if that is how Python 3 is on your `PATH`).

## Prerequisites

What you need depends on which scripts you run.

| Tool | Used by |
|------|--------|
| [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v5.x (`idf.py` on PATH, `export` sourced); `IDF_TARGET` respected | `build_flash.*`, `qemu_run.py` |
| [`esptool.py`](https://docs.espressif.com/projects/esptool/) (normally with ESP-IDF) | `build_flash.*` when `--factory` merges & flashes merged image |
| Serial port (`COMn` on Windows, `/dev/ttyUSB*` on Linux, etc.) | `build_flash.*` |
| [CMake](https://cmake.org/) 3.16+ | `demo.ps1`, `demo.sh` |
| [Tinkwell `tw` CLI](../../../../README.md) on PATH (`tw firmwareless-hub`, `tw coap ...`) | `demo.*`, `fake_hub.py`, `provision.py` (LAN/SoftAP transports) |
| Python 3.7+ (`python` / `python3`) | `fake_hub.py`, `fake_sensor.py`, `provision.py`, `qemu_run.py` |
| [`bleak`](https://pypi.org/project/bleak/) (`pip install bleak`) | `provision.py` only for BLE transport (`ble` subcommands) |
| QEMU / `idf.py qemu` (ESP-IDF integration; see [`qemu-testing.md`](../docs/guides/qemu-testing.md)) | `qemu_run.py` |

ESP-IDF applies to flashing and QEMU-based runs.
Building the **`tw`** CLI from the core repo uses `.NET`; see the [workspace README](../../../../README.md) and the device [quick start](../docs/getting-started/quick-start.md) for invoking `tw` with these scripts.

## `build_flash.ps1` / `build_flash.sh`

**Purpose.** Build one of the bundled ESP-IDF examples under `examples/<name>/esp-idf` and flash it to connected hardware.

**Synopsis.**

```powershell
# From device/ ; Windows defaults: COM3, esp32c3
.\scripts\build_flash.ps1 -Example thermostat [-Factory] [-Port COM6] [-Target esp32c6]
```

```bash
./scripts/build_flash.sh <example> [--factory] [--port <port>] [--target <target>]
```

**Arguments.**

- **`<example>` / `-Example`** (required except when showing usage): example folder name (`thermostat`, `applet-device`, …).
- **`--factory` / `-Factory`:** build the [factory app](../factory), copy `partitions_factory.csv` into the example, merge bootloader, partition table, factory and app binaries with `esptool.py merge_bin`, then flash merged image (`0x0`).
- **`--port` / `-Port`:** serial port (`build_flash.ps1` defaults to `COM3`; `build_flash.sh` uses `ESPPORT` if set, else `/dev/ttyUSB0`).
- **`--target` / `-Target`:** chip (`build_flash.ps1` default `esp32c3`; Bash uses `IDF_TARGET` if set in the environment, else `esp32c3`).

**Examples.**

```bash
./scripts/build_flash.sh thermostat
./scripts/build_flash.sh applet-device --factory --port /dev/ttyACM0 --target esp32c3
```

**See also.** [ESP-IDF guide](../docs/guides/esp-idf.md), [Quick start](../docs/getting-started/quick-start.md).

## `demo.ps1` / `demo.sh`

**Purpose.** End-to-end local demo for the POSIX path: CMake-build the thermostat (`PAL_BACKEND=posix`), start `tw firmwareless-hub start`, launch the thermostat binary, optionally probe `/tw/info` via `tw coap send`.

**Synopsis.**

```powershell
.\scripts\demo.ps1
```

```bash
./scripts/demo.sh
```

**Arguments.** Neither script exposes parameters; behaviors are fixed in the scripts (thermostat path, `PAL_BACKEND=posix`, hub/device startup).

**Examples.** Run from **`device/`** as `./scripts/demo.sh` or `.\scripts\demo.ps1`; the scripts resolve **`examples/thermostat`** relative to **`device/`** via their location on disk, not your current shell directory.

**See also.** [Quick start](../docs/getting-started/quick-start.md).

## `fake_hub.py`

**Purpose.** Development stand-in for a hub: wraps `tw coap server` with `--mailbox`, handling heartbeats and dispatching queued commands as CoAP POSTs to the device.

**Synopsis.**

```bash
python3 scripts/fake_hub.py [--port N] [--prefix PREFIX] [-v|--verbose] [--queue CMD[:json]] [--queue ...]
```

**Arguments.**

- **`--port`:** UDP port (default **5684**).
- **`--prefix`:** CoAP path prefix for commands (default **`tw`**).
- **`--verbose` / `-v`:** pass `--log-payload` through to `tw coap server`.
- **`--queue`:** repeatable; pre-queues a command string such as `reboot` or `set-config:{"entries":[...]}`.

**Examples.**

```bash
python3 scripts/fake_hub.py
python3 scripts/fake_hub.py --port 5684 --verbose --queue reboot
```

**See also.** [Wire specification](../docs/protocol/wire-specification.md), [Quick start](../docs/getting-started/quick-start.md) (CoAP probes).

## `fake_sensor.py`

**Purpose.** Generate fake temperature/humidity traces for **POSIX** development by updating `TW_FAKE_TEMP` and `TW_FAKE_HUMID` in the script process (tenths of °C and %RH, e.g. `215` → 21.5°C).
Wave and random modes mutate only this script’s environment; pair them with the same-shell patterns from [quick start](../docs/getting-started/quick-start.md) if you need the running thermostat to see updates.
For a single static run, set `TW_FAKE_TEMP` / `TW_FAKE_HUMID` when launching `./build/thermostat` there.

**Synopsis.**

```bash
python3 scripts/fake_sensor.py [--temp T] [--humid H] [--wave] [--period SEC] [--min A] [--max B] [--random] [--start S] [--step N]
```

**Modes.**

- Default: static `--temp` / `--humid`, keep process alive.
- **`--wave`:** sinusoidal temperature between `--min` and `--max` over `--period` seconds; fixed humidity from `--humid`.
- **`--random`:** random walk from `--start` with step up to `--step`.

**Examples.**

```bash
python3 scripts/fake_sensor.py --temp 215 --humid 450
python3 scripts/fake_sensor.py --wave --period 120 --min 180 --max 260
```

**See also.** [Quick start](../docs/getting-started/quick-start.md) (simulated temperatures).

## `provision.py`

**Purpose.** Interactive and non-interactive provisioning CLI: binary CoAP via `tw coap send` (LAN, SoftAP) or kvtext over BLE GATT when `bleak` is installed.

**Synopsis.**

```bash
python3 scripts/provision.py [--json] [--verbose] [--timeout SEC] <transport> [<command>] ...
```

Transport-specific arguments (for **`lan`** / **`softap`** / **`ble`**) precede **`command`** or follow **`--help`** for each hierarchy.
Omit **`<command>`** for **`lan`**, **`softap`**, or **`ble`** to run interactively (**`TW Provision>`** prompts).
Using **`python3 scripts/provision.py ble --scan`** lists devices without sending **`factory`** / **`hub`** / **`info`** payloads.

**Examples.**

```bash
python3 scripts/provision.py lan info --host 127.0.0.1 --port 5683
python3 scripts/provision.py lan hub --ssid MY_NET --password secret --hub-url coap://192.168.1.10:5684
python3 scripts/provision.py softap hub --ssid MY_NET --password secret --hub-url coap://192.168.1.10:5684
python3 scripts/provision.py ble --scan
python3 scripts/provision.py --json lan info --host 127.0.0.1 --port 5683
```

**See also.** [Provisioning](../docs/guides/provisioning.md) (lifecycle and concepts—not duplicated here).

## `qemu_run.py`

**Purpose.** Launch QEMU via `python -m idf_py qemu monitor` for a given `--project` ESP-IDF directory.
It configures user-mode NIC forwarding (`hostfwd`), waits for boot log text (`entering main loop`), optionally runs `--test`, and sets `TW_DEVICE_HOST` / `TW_DEVICE_PORT` for that test subprocess.

**Synopsis.**

```bash
python3 scripts/qemu_run.py --project <esp-idf-dir> [--forward PORT] [--timeout SEC] [--test "shell command"]
```

**Environment.** **`IDF_PATH`** must point at ESP-IDF, or **`~/esp/esp-idf`** must exist.

**Examples.**

```bash
python3 scripts/qemu_run.py --project examples/thermostat/esp-idf --forward 5683
python3 scripts/qemu_run.py --project examples/thermostat/esp-idf --test "pytest test/integration/"
```

**See also.** [QEMU testing](../docs/guides/qemu-testing.md).

## Cross-platform notes (`.ps1` vs `.sh`)

[`build_flash.ps1`](build_flash.ps1) and [`build_flash.sh`](build_flash.sh), and [`demo.ps1`](demo.ps1) and [`demo.sh`](demo.sh), implement the **same workflows** on Windows PowerShell versus Bash.
Default serial ports differ (`COM3` versus `/dev/ttyUSB0`).
Bash honors `ESPPORT` / `IDF_TARGET` where documented in `build_flash.sh`.
`demo.ps1` starts child processes hidden and stops them in a `finally` block on exit.
`demo.sh` uses `trap` and colored output on a TTY.
The Python scripts are cross-platform assuming `tw` and Python resolve on `PATH`.

## Local dev workflow (typical combo)

Run all of this from **`device/`** (see [Canonical working directory](#canonical-working-directory)).

For **POSIX / no hardware:** run [`demo.sh`](demo.sh) or [`demo.ps1`](demo.ps1).
Or run `cmake --build` on the thermostat yourself, then `tw firmwareless-hub start` and `./build/thermostat`.
Use [`fake_hub.py`](fake_hub.py) when exercising mailbox-centric hub behavior without a full hub stack.
Use [`fake_sensor.py`](fake_sensor.py) or inline `TW_FAKE_TEMP=` as in [quick start](../docs/getting-started/quick-start.md) to vary readings.

For **real ESP32:** install and source ESP-IDF, then [`build_flash.sh`](build_flash.sh) / [`build_flash.ps1`](build_flash.ps1); see [ESP-IDF guide](../docs/guides/esp-idf.md).

For **provision** over LAN, SoftAP, or BLE, run [`provision.py`](provision.py); see [Provisioning](../docs/guides/provisioning.md).

For **QEMU:** build for a supported chip (see [QEMU testing](../docs/guides/qemu-testing.md)), then [`qemu_run.py`](qemu_run.py).

Applets-on-device workflows align with [Your first applet](../docs/getting-started/your-first-applet.md).
Branching from templates is covered in [Build your own device](../docs/getting-started/build-your-device.md).

When something fails (ports, timeouts, flash size), check [Troubleshooting](../docs/guides/troubleshooting.md).
