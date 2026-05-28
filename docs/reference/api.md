# API Reference

## tw_device.h -- Device Descriptor

### `tw_device_config_t`

```c
typedef struct tw_device_config {
    const char           *name;                // Device name (e.g. "thermostat")
    const char           *fw_version;          // Firmware version string

    /* Compile-time identity defaults (overridable by factory provisioning) */
    int32_t               vendor_id;
    int32_t               product_id;
    const char           *vendor_display_name;
    const char           *product_display_name;
    uint8_t               variant;

    tw_msg_resource_t    *resources;           // NULL-terminated message resource table
    tw_err_t (*on_init)(const tw_device_config_t *dev);
    tw_err_t (*on_tick)(const tw_device_config_t *dev);
    tw_err_t (*on_wake)(const tw_device_config_t *dev);
    tw_err_t (*on_sleep)(const tw_device_config_t *dev);
    tw_err_t (*on_command)(const struct tw_device_config *dev,
                           const char *command,
                           const uint8_t *payload, size_t payload_len);
    tw_heartbeat_fn_t    heartbeat_payload;    // Optional: append to heartbeat
    tw_err_t (*on_provision)(const tw_device_config_t *dev);
    uint32_t              tick_interval_ms;     // Main loop tick period
    void                 *user_data;           // Opaque app pointer
} tw_device_config_t;
```

**`on_command`** is optional. When set, it receives hub-pushed command notifications with a short `command` name and raw `payload` (typically protobuf); see **tw_cmd.h** below for `svc_cmd_resources[]` and the **Protobuf heartbeat messages** section for `proto/tw_protocol.proto`.

### `tw_device_run(const tw_device_config_t *cfg)`

Initialises the PAL, identity service, transport, protocol backend (including SDK-internal resources `/tw/info` and `/tw/identity/pubkey`), heartbeat service, and enters the main loop.
In always-on mode the loop runs until `tw_device_request_shutdown()` is called, then returns `TW_OK` after clean deinitialisation.
In deep-sleep mode (`CONFIG_TW_POWER_DEEP_SLEEP=y`) each wake cycle performs a single iteration and enters deep sleep.

### `TW_DEVICE_MAIN(cfg)`

Macro that expands to `app_main()` on ESP-IDF or `int main()` on POSIX.

---

## tw_identity.h -- Device Identity

### Types

```c
typedef enum {
    TW_KEY_NONE    = 0,
    TW_KEY_ED25519 = 1,
    TW_KEY_PSK     = 2,
} tw_identity_key_type_t;

typedef struct {
    int32_t  vendor_id;
    int32_t  product_id;
    char     vendor_display_name[48];
    char     product_display_name[48];
    uint8_t  variant;
    int32_t  serial_number;
    uint8_t  uuid[16];
    bool     uuid_valid;
    bool     factory_provisioned;
    bool     hub_provisioned;
    tw_identity_key_type_t key_type;
    uint8_t  key[32];
    bool     key_valid;
} tw_device_identity_t;
```

### `tw_identity_init(cfg)`

Loads identity from compile-time defaults in `cfg` and NVS overrides.
Called automatically by `tw_device_run()`.

### `tw_identity_get()`

Returns a pointer to the in-memory identity struct (read-only).

### `tw_identity_set_uuid(uuid)` / `tw_identity_get_uuid(uuid_out)`

UUID accessors.
`set_uuid` persists to NVS.

### `tw_identity_uuid_to_str(uuid, buf, buf_size)`

Formats a 16-byte UUID as `"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"`.
`buf` must be at least `TW_UUID_STR_SIZE` (37) bytes.

### `tw_identity_get_key_type()` / `tw_identity_has_key()`

Query the configured identity key type and whether a key is loaded.

### `tw_identity_get_ed25519_pubkey(pubkey_out)`

Derives the 32-byte Ed25519 public key from the stored private key.
Returns `TW_ERR_INVAL` if key type is not Ed25519, `TW_ERR_NOT_READY` if no key is loaded.
Uses PSA Crypto on ESP-IDF, stub on POSIX.

### `tw_identity_set_factory(...)`

Writes all factory identity fields to the in-memory struct and NVS.
Sets `factory_provisioned = true`.
Does not set `hub_provisioned`.

### `tw_identity_set_field(key, value, len)`

Runtime update of a single identity field.

---

## tw_msg.h -- Message resources and protocol vtable

Wire-protocol-neutral request/response/resource types and helpers.
The compile-time selected backend (CoAP today; MQTT planned) implements `tw_protocol_t`.

### `tw_msg_resource_t`

```c
typedef struct {
    const char       *path;
    uint8_t           methods;
    tw_msg_handler_t handler;
} tw_msg_resource_t;
```

### `tw_msg_request_t`

```c
typedef struct {
    uint8_t         method;
    const char     *path;
    const uint8_t  *payload;
    size_t          payload_len;
    const char     *query;
    /* Block1 (RFC 7959) -- set by protocol backend when present */
    bool            has_block1;
    uint32_t        block1_num;
    bool            block1_more;
    uint16_t        block1_szx;
} tw_msg_request_t;
```

### `tw_msg_response_t`

```c
typedef struct {
    uint8_t   code;
    uint8_t  *payload;
    size_t    payload_len;
    size_t    payload_capacity;
    /* Block1 echo -- handler sets these for block-at-a-time responses */
    bool      set_block1;
    uint32_t  block1_num;
    bool      block1_more;
    uint16_t  block1_szx;
} tw_msg_response_t;
```

### Response helpers

- `tw_msg_respond_with_code(resp, code, diagnostic)` -- set response code with optional diagnostic text
- `tw_msg_respond_text(resp, text)` -- text/plain body (code = 2.05 Content)
- `tw_msg_respond_i32(resp, value)` -- integer body (code = 2.05 Content)
- `tw_msg_respond_buf(resp, data, len)` -- raw bytes body (code = 2.05 Content)
- `tw_msg_respond_empty(resp, code)` -- empty body with code

`tw_msg_respond_with_code()` is the primary response helper for binary CoAP.
It sets `resp->code` directly (encoded in the CoAP header) and optionally writes a diagnostic string as the response payload.

### Response codes

Numeric values match CoAP for interoperability: `TW_MSG_201_CREATED`, `TW_MSG_202_DELETED`, `TW_MSG_204_CHANGED`, `TW_MSG_205_CONTENT`, `TW_MSG_231_CONTINUE`, `TW_MSG_400_BAD_REQ`, `TW_MSG_404_NOT_FOUND`, `TW_MSG_405_NOT_ALLOWED`, `TW_MSG_408_INCOMPLETE`, `TW_MSG_413_TOO_LARGE`, `TW_MSG_500_INTERNAL`.

### `tw_block_ctx_t` -- Opt-in Block1 reassembly

```c
typedef struct {
    uint8_t *buf;
    size_t   buf_size;
    size_t   received;
    uint32_t next_num;
} tw_block_ctx_t;

void     tw_block_init(tw_block_ctx_t *ctx, uint8_t *buf, size_t buf_size);
tw_err_t tw_block_feed(tw_block_ctx_t *ctx,
                       tw_msg_request_t *req,
                       tw_msg_response_t *resp);
```

Application handlers that want full-payload reassembly in RAM can use `tw_block_ctx_t`.
Call `tw_block_init()` once, then `tw_block_feed()` for each request.
It returns `TW_ERR_NOT_READY` (2.31 Continue) while blocks arrive and `TW_OK` when the final block completes reassembly.
The SDK's own OTA and applet services do NOT use this -- they stream to flash directly with manual block sequence validation.

Method flags: `TW_MSG_GET`, `TW_MSG_POST`, `TW_MSG_PUT`, `TW_MSG_DELETE` (combinable bitmask).

### `TW_MSG_RESOURCE_END`

Sentinel value to terminate resource tables: `{ NULL, 0, NULL }`.

### `tw_protocol_t` and `tw_protocol_create()`

```c
typedef struct tw_protocol {
    tw_err_t (*init)(struct tw_protocol *self,
                     tw_msg_resource_t *resources, uint16_t port);
    void (*poll)(struct tw_protocol *self, int timeout_ms);
    void (*deinit)(struct tw_protocol *self);
    tw_err_t (*send)(struct tw_protocol *self,
                     const char *host, uint16_t port,
                     const char *path,
                     const uint8_t *payload, size_t payload_len,
                     uint8_t *resp_buf, size_t resp_buf_size,
                     size_t *resp_len, int timeout_ms);
} tw_protocol_t;

tw_protocol_t *tw_protocol_create(void);
```

`tw_protocol_create()` returns a pointer to the static vtable for the backend selected in Kconfig (`TW_PROTOCOL`).
Do not free the returned pointer.
Services call `init` with the application resource table, `poll` from the main loop, and `send` for outbound hub traffic (e.g. heartbeats).

---

## tw_cmd.h -- Hub-pushed command resources

### `svc_cmd_resources[]`

Static `tw_msg_resource_t` table (from `tw_cmd.h`) listing CoAP **POST** handlers for commands the hub pushes to the device after a heartbeat.
Paths are built as `/<prefix>/<command>` where `<prefix>` is `CONFIG_TW_CMD_PATH_PREFIX` (default `"tw"`).
The SDK merges this table with the application `resources` from `tw_device_config_t` when starting the protocol server.

Built-in endpoints (default prefix) include orderly reboot, configuration updates, OTA availability signalling, and an application hook (`app`).
Each handler decodes its body with nanopb when `CONFIG_TW_USE_PROTOBUF=y`.

Terminated by `TW_MSG_RESOURCE_END`.

### `on_command` (in `tw_device_config_t`)

Optional callback invoked for application-level hub commands and as a notification path for other hub-pushed commands, depending on build options:

```c
tw_err_t (*on_command)(const struct tw_device_config *dev,
                       const char *command,
                       const uint8_t *payload, size_t payload_len);
```

- `command` — short name derived from the CoAP path (e.g. `"app"`, `"set-config"`).
- `payload` / `payload_len` — request body bytes (typically protobuf when protobuf support is enabled).

---

## Protobuf heartbeat messages (`proto/tw_protocol.proto`)

The canonical message definitions live in **`proto/tw_protocol.proto`** in the SDK tree.
The SDK uses **nanopb** for encode/decode on the device.

**`HeartbeatPayload`** (device → hub on `POST /hub/heartbeat`) carries device identity and health: GUID, vendor/product IDs, serial, firmware string, uptime, free heap, boot reason, optional opaque `app_data`, and an optional repeated set of sensor readings when sensor-in-heartbeat features are enabled.

**`HeartbeatReply`** (hub → device in the `2.05 Content` response) carries a **`pending`** count: how many hub-pushed CoAP commands will follow in the current cycle, which drives the post-heartbeat listen window.

---

## tw_types.h -- Error Codes & Utilities

| Code | Value | Meaning |
|------|-------|---------|
| `TW_OK` | 0 | Success |
| `TW_ERR_NOMEM` | -1 | Out of memory |
| `TW_ERR_INVAL` | -2 | Invalid argument |
| `TW_ERR_IO` | -3 | I/O error |
| `TW_ERR_TIMEOUT` | -4 | Operation timed out |
| `TW_ERR_BUSY` | -5 | Resource busy |
| `TW_ERR_NOT_FOUND` | -6 | Key/resource not found |
| `TW_ERR_NOT_READY` | -7 | Not initialised |
| `TW_ERR_REFUSED` | -8 | Operation refused |
| `TW_ERR_OVERFLOW` | -9 | Buffer overflow |
| `TW_ERR_CANCELLED` | -10 | Operation cancelled |

Utility macros: `TW_UNUSED`, `TW_ARRAY_SIZE`, `TW_MIN`, `TW_MAX`, `TW_CLAMP`.

---

## tw_hub.h -- Hub Communication

### `tw_hub_set_address(coap_uri)`

Set the hub CoAP address.
Stored in NVS.
Format: `"coap://host:port"`.

### `tw_hub_get_address(buf, size)`

Retrieve the current hub address.

---

## tw_ota.h -- OTA Status

### `tw_ota_state()` / `tw_ota_progress_pct()`

Read-only access to the current OTA state and progress percentage.

States: `IDLE`, `RECEIVING`, `VERIFYING`, `REBOOTING`, `PENDING_VERIFY`, `ERROR`.

---

## tw_button.h -- Button Utility

### `tw_button_create(btn, pin, callback, ctx)`

Configures a GPIO pin with pull-up and registers a debounced press callback.

### `tw_button_poll(btn)`

Call from the tick loop.
Reads GPIO, debounces, invokes callback on press.

---

## tw_led.h -- LED Utility

### `tw_led_create(led, pin)`

Configures a GPIO pin as output for LED control.

### `tw_led_set_pattern(led, pattern)`

Patterns: `TW_LED_OFF`, `TW_LED_SOLID`, `TW_LED_BLINK_SLOW`, `TW_LED_BLINK_FAST`.

---

## tw_safety.h -- Safety Monitor

### `tw_safety_create(mon, cfg)`

Initialise with low/high thresholds and callbacks.

### `tw_safety_check(mon, value)`

Check a reading against thresholds.
Invokes `on_low` or `on_high` callback and sets the override flag.

### `tw_safety_is_overriding(mon)`

Returns `true` if a safety override is currently active.

---

## tw_config.h -- Config Helpers

### `tw_config_get_i32(key, default)` / `tw_config_set_i32(key, value)`

Typed NVS wrappers for int32 configuration values.

### `tw_config_get_str(key, buf, size)` / `tw_config_set_str(key, value)`

Typed NVS wrappers for string configuration values.

---

## tw_sensor.h -- Sensor Service

### `tw_sensor_read_int(name, out_value)`

```c
tw_err_t tw_sensor_read_int(const char *name, int32_t *out_value);
```

Read a named sensor value.
Returns `TW_OK` on success with the value written to `*out_value`, `TW_ERR_NOT_FOUND` if the sensor name is not registered, or a driver error code.

### `tw_device_request_shutdown()`

Request a graceful exit from the main loop.
The loop finishes its current iteration, deinitializes the protocol, flushes NVS, then `tw_device_run()` returns `TW_OK`.
Safe to call from any thread or handler.

---

## tw_kvtext.h -- Provisioning Wire Format

### `tw_kvtext_parse(buf, len, cb, ctx)`

Parse a kvtext buffer, calling `cb` for each valid `key=value` pair.

### `tw_kvtext_write_str(buf, cap, pos, key, value)`

Append `key=value\n` to the buffer.
Returns `TW_ERR_OVERFLOW` if the line won't fit.

### `tw_kvtext_write_i32(buf, cap, pos, key, value)`

Append `key=<integer>\n` to the buffer.

### `tw_kvtext_write_hex(buf, cap, pos, key, data, data_len)`

Append `key=<hex encoded>\n` to the buffer.

---

## tw_lock.h -- Thread Safety

When `CONFIG_TW_THREAD_SAFETY=y` (default), these expand to `pal_mutex_*` calls.
When disabled, they are no-ops.

### `tw_lock_init(lock)` / `tw_lock_acquire(lock)` / `tw_lock_release(lock)` / `tw_lock_destroy(lock)`

---

## tw_applet.h -- Applet Runtime

### `tw_applet_state()`

Returns: `TW_APPLET_NONE`, `TW_APPLET_LOADING`, `TW_APPLET_RUNNING`, `TW_APPLET_ERROR`.

### `tw_applet_version()`

Returns the version string of the currently loaded applet, or `NULL`.
