# Troubleshooting

Common issues and how to fix them.

---

## Build issues

### CMake fails fetching libcoap

**Symptom:** `FetchContent` errors during the first build.

**Fix:** Ensure you have internet access and CMake 3.16+.
If behind a proxy, set `HTTP_PROXY` / `HTTPS_PROXY` environment variables.
If using a corporate firewall, `git` must be able to reach GitHub over HTTPS.

### Build runs out of flash on ESP32

**Symptom:** Linker error about section overflow or `idf.py size` exceeds partition.

**Fix:** Disable features you don't need in `menuconfig`:
- `CONFIG_TW_TRANSPORT_BLE=n` saves ~100 KB flash, ~35 KB RAM
- `CONFIG_TW_APPLET_ENABLED=n` removes the WAMR runtime (~50 KB)
- `CONFIG_TW_TRANSPORT_THREAD=n` removes OpenThread

### `idf.py` not found

**Fix:** Source the ESP-IDF export script:
```bash
. $HOME/esp/esp-idf/export.sh
```

---

## Runtime issues

### CoAP queries hang or time out

**Symptom:** `tw coap send get /tw/sensor/temperature` never returns.

**Causes:**
- Device isn't running.
  Check the terminal for startup logs.
- Wrong port.
  The device listens on UDP 5683 by default.
- Firewall blocking UDP.
  On Linux, check `iptables` / `ufw`.
- QEMU port forwarding not set up (use `hostfwd=udp::15683-:5683`).

### NVS file corruption or stale state

**Symptom:** Device behaves unexpectedly after code changes (wrong mode, old WiFi credentials, identity mismatch).

**Fix:** Delete the POSIX NVS file and restart:
```bash
rm -rf ~/.tw-device
```

On ESP32, erase the NVS partition:
```bash
idf.py erase-flash
```

### Device prints "no hub" or heartbeat fails

**Symptom:** `W (xxx) [heartbeat] no hub address configured`

**Fix:** The device needs a hub URL.
Either:
- Run [`fake_hub.py`](../../scripts/fake_hub.py) for local development.
- Set the hub address via provisioning (`provision.py hub`).
- Set the `hub-url` key via `tw coap send put /tw/set-config`.

---

## Provisioning issues

### BLE provisioning times out

**Causes:**
- `bleak` not installed: `pip install bleak`
- Device not in provisioning mode.
  On ESP32, it must boot into the provisioning partition (hold reset button, or flash with `--factory`).
- Wrong BLE address.
  Use `--scan` to discover devices.

### SoftAP not visible

**Symptom:** No WiFi network named `TW-Prov-XXXX`.

**Causes:**
- Device is already provisioned (main app partition doesn't run SoftAP).
  Reset provisioning via GPIO button or NVS erase.
- `CONFIG_TW_PROVISION_SOFTAP=n` in the provisioning partition's `sdkconfig`.

---

## QEMU issues

### QEMU networking not working

**Symptom:** Can't reach the device from the host, or the device can't reach the hub.

**Fix (user-mode networking):**
```bash
idf.py qemu monitor -- -nic user,hostfwd=udp::15683-:5683
```
Then query on port 15683: `tw coap send get /tw/sensor/temperature --port 15683`

**Fix (TAP networking):**
```bash
sudo ip tuntap add dev tap0 mode tap user $(whoami)
sudo ip addr add 192.168.7.1/24 dev tap0
sudo ip link set tap0 up
idf.py qemu monitor -- -nic tap,ifname=tap0,script=no,downscript=no
```

### QEMU crashes on non-C3 target

**Fix:** QEMU support is ESP32-C3 only.
Set target before building:
```bash
idf.py set-target esp32c3
```

---

## OTA issues

### OTA transfer fails mid-way

**Symptom:** `4.08 Request Entity Incomplete` or transfer stalls.

**Causes:**
- Block1 sequence error (blocks sent out of order).
- Packet loss on the network.
  CoAP retransmits automatically, but severe loss may exceed retry limits.
- Image size exceeds the declared size in `POST /tw/ota/begin`.

### OTA rolls back after reboot

**Symptom:** Device boots new firmware but reverts to old version.

**Fix:** The rollback timer (`CONFIG_TW_OTA_ROLLBACK_TIMEOUT_S`, default 60s) expired before `pal_ota_mark_valid()` was called.
Ensure the first heartbeat succeeds within the timeout.
Increase the timeout if the device takes a long time to connect.

---

## Applet issues

### Applet too large

**Symptom:** `4.13 Request Entity Too Large` when pushing an applet.

**Fix:** Increase `CONFIG_TW_APPLET_MAX_SIZE` (default 64 KB) or optimise the applet.
The blinky examples are ~120--200 bytes; a thermostat is ~2--4 KB.

### Applet crashes or doesn't start

**Symptom:** Logs show `applet_init failed` or `WAMR load error`.

**Causes:**
- The `.wasm` file is invalid or compiled for the wrong target (must be `wasm32`).
- The applet imports a function not in the host API.
- WASM stack or heap too small.
  Increase `CONFIG_TW_WASM_STACK_SIZE` / `CONFIG_TW_WASM_HEAP_SIZE`.

---

## Still stuck?

- Check the [architecture overview](../architecture/overview.md) to understand which component owns the behavior.
- Search the [Kconfig reference](../reference/kconfig.md) for the relevant option.
- For wire-level debugging, inspect traffic with `tw coap send get /tw/info` or a CoAP packet capture.
