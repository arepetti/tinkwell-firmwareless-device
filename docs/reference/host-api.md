# Host API Reference

These are the native functions that the device runtime exports to the WASM sandbox.
Every applet, regardless of source language, calls these through the WASM import mechanism.

All functions are imported from the `"env"` module.

## String convention

All string parameters use **length-prefixed** encoding: `(ptr: i32, len: u32)` where `ptr` is an offset into WASM linear memory and `len` is the byte count.
Strings are **NOT** NUL-terminated.

## Memory safety

WAMR's native binding translates a single `ptr` from app address to native pointer, but does **not** validate that the full range `[ptr .. ptr+len)` lies within linear memory.
All `_s*` host function implementations must call `wasm_runtime_validate_app_addr(module_inst, ptr, len)` before reading the buffer.
If the range is out-of-bounds, the call returns a safe default (0 / 0.0f for reads) and is silently dropped (for writes/logs).
String length is also capped at 128 bytes on the host side.

## Naming convention

Variant functions use a two-letter suffix `_XY` where:

| Position | Value | Meaning |
|----------|-------|---------|
| X (input) | `s` | String name (ptr + len) |
| X (input) | `n` | Numeric i32 ID |
| Y (return) | `f` | f32 result |
| Y (return) | `n` | i32 result |

## Sensor access

### `tw_host_read_sensor_sf(name: ptr, name_len: u32) -> f32`

Read sensor by string name, return f32.

### `tw_host_read_sensor_sn(name: ptr, name_len: u32) -> i32`

Read sensor by string name, return i32.

### `tw_host_read_sensor_nf(sensor_id: i32) -> f32`

Read sensor by numeric ID, return f32.

### `tw_host_read_sensor_nn(sensor_id: i32) -> i32`

Read sensor by numeric ID, return i32.

## GPIO

### `tw_host_write_gpio(pin: i32, value: i32)`

Set a GPIO pin to high (1) or low (0).

### `tw_host_read_gpio(pin: i32) -> i32`

Read the current state of a GPIO pin.
Returns 0 or 1.

## LEDs

### `tw_host_set_led(pin: i32, pattern: i32)`

Set an LED pattern.

| Pattern value | Meaning |
|---------------|---------|
| 0 | Off |
| 1 | Solid on |
| 2 | Blink slow (~500 ms) |
| 3 | Blink fast (~150 ms) |
| 4 | Single pulse |

## Logging

### `tw_host_log_s(msg: ptr, msg_len: u32, level: i32)`

Log a message by string content.

- **msg, msg_len**: Pointer and length of the message in WASM linear memory
- **level**: 0=error, 1=warn, 2=info, 3=debug

### `tw_host_log_n(msg_id: i32, level: i32)`

Log a message by numeric ID (for string-table-based logging).

## Configuration (NVS)

### i32 variants

#### `tw_host_config_get_i32_s(key: ptr, key_len: u32, def: i32) -> i32`

Read an integer config value by string key.
Returns `def` if the key doesn't exist.

#### `tw_host_config_get_i32_n(key_id: i32, def: i32) -> i32`

Read an integer config value by numeric key ID.

#### `tw_host_config_set_i32_s(key: ptr, key_len: u32, val: i32)`

Write an integer config value by string key.

#### `tw_host_config_set_i32_n(key_id: i32, val: i32)`

Write an integer config value by numeric key ID.

### f32 variants

#### `tw_host_config_get_f32_s(key: ptr, key_len: u32, def: f32) -> f32`

Read a float config value by string key.
Returns `def` if the key doesn't exist.
Used by compiled state machines when params are declared with `as "f32"` or when `--data-type` is `f32`/`f64`.

#### `tw_host_config_get_f32_n(key_id: i32, def: f32) -> f32`

Read a float config value by numeric key ID.

#### `tw_host_config_set_f32_s(key: ptr, key_len: u32, val: f32)`

Write a float config value by string key.

#### `tw_host_config_set_f32_n(key_id: i32, val: f32)`

Write a float config value by numeric key ID.

### Usage by compiled state machines

The `params` block in a state machine DSL compiles to `tw_host_config_get_*` calls.
The compiler selects the variant based on `--data-type` and the `as` modifier:

| Data type | Config function |
|-----------|----------------|
| `f64`, `f32` | `tw_host_config_get_f32_*` |
| `i32`, `i16` | `tw_host_config_get_i32_*` |
| `as "f32"` override | `tw_host_config_get_f32_*` |
| `as "i32"` override | `tw_host_config_get_i32_*` |

The `access-by` modifier controls how the param is resolved at runtime:

| `access-by` | Behavior |
|-------------|----------|
| `"name"` (default) | String key via `_s` variant |
| `"index"` | Declaration-order index via `_n` variant |
| `"const"` | Value inlined as `*.const`; **no config call emitted** |

Parameters declared with `access-by "const"` are compile-time constants -- they do not generate any `tw_host_config_get_*` imports or calls.
Their values are baked directly into the WASM module as constant instructions.

## Time

### `tw_host_uptime_ms() -> i64`

Returns the monotonic uptime in milliseconds.
Wraps the PAL's `pal_uptime_ms()`.
Used by compiled state machines for timeout and debounce tracking.

## Condition trap

### `tw_trap(condition_id: i32)`

Called when a compiled state machine **precondition** or **postcondition** is violated at runtime.
The `condition_id` maps to an entry in the companion manifest JSON, enabling the host to log a meaningful diagnostic message including the condition type, machine name, state name, and source line.

Typical host implementation: log the violation at `CRITICAL` level and halt the offending machine (or the entire applet, depending on policy).

## State machine callback

### `sm_on_transition(machine_id: i32, from_id: i32, to_id: i32)`

Called on every state transition in a compiled state machine module.

- **machine_id**: Which machine transitioned (0-based index)
- **from_id**: Previous state ID
- **to_id**: New state ID

## Applet exports

The applet must export these functions:

### `applet_init()`

Called once when the applet is loaded (at boot or after a hot-swap).

### `applet_tick()`

Called every tick interval (default: 1000 ms).
This is where the applet reads sensors, applies logic, and sets outputs.

### `applet_on_command(type: i32, data: ptr, len: i32)`

Called when the hub sends an application command (`TW_CMD_APP`).

- **type**: Command type (>= 128)
- **data**: Pointer to command payload in WASM linear memory
- **len**: Payload length in bytes

### State machine exports (optional)

### `sm_get_state(machine_id: i32) -> i32`

Query a machine's current state ID.

### `sm_machine_count() -> i32`

Returns the number of state machines in the module.
