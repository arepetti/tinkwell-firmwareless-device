/**
 * Thermostat applet in AssemblyScript.
 *
 * Compiled with: npx asc assembly/index.ts -o build/thermostat.wasm
 *
 * This is the same OFF/ON/AUTO state machine as the compiled C thermostat
 * example, but running inside the WASM interpreter on the device.
 */

// Host function imports (provided by the device runtime)
@external("env", "tw_host_read_sensor_sn")
declare function tw_host_read_sensor_sn(name: usize, name_len: u32): i32;

@external("env", "tw_host_write_gpio")
declare function tw_host_write_gpio(pin: i32, value: i32): void;

@external("env", "tw_host_set_led")
declare function tw_host_set_led(pin: i32, pattern: i32): void;

@external("env", "tw_host_log_s")
declare function tw_host_log_s(msg: usize, msg_len: u32, level: i32): void;

@external("env", "tw_host_config_get_i32_s")
declare function tw_host_config_get_i32_s(key: usize, key_len: u32, def: i32): i32;

@external("env", "tw_host_config_set_i32_s")
declare function tw_host_config_set_i32_s(key: usize, key_len: u32, val: i32): void;

// Pin assignments (must match the board)
const PIN_RELAY:     i32 = 3;
const PIN_LED_MODE:  i32 = 1;
const PIN_LED_RELAY: i32 = 2;

// LED patterns
const LED_OFF:        i32 = 0;
const LED_SOLID:      i32 = 1;
const LED_BLINK_SLOW: i32 = 2;

// Safety thresholds (tenths of C)
const FREEZE_THRESHOLD:   i32 = 30;   // 3.0 C
const OVERHEAT_THRESHOLD: i32 = 400;  // 40.0 C

// State
let mode: i32 = 2;  // 0=OFF, 1=ON, 2=AUTO
let relayState: bool = false;
let relayRequested: bool = false;

// Encoded string helpers (AssemblyScript strings -> WASM memory)
const tempSensor = String.UTF8.encode("temperature");
const modeKey    = String.UTF8.encode("mode");

export function applet_init(): void {
    mode = tw_host_config_get_i32_s(changetype<usize>(modeKey), modeKey.byteLength, 2);
}

export function applet_tick(): void {
    const temp = tw_host_read_sensor_sn(changetype<usize>(tempSensor), tempSensor.byteLength);

    // Safety overrides
    let safetyOverride = false;
    if (temp <= FREEZE_THRESHOLD) {
        relayState = true;
        safetyOverride = true;
    } else if (temp >= OVERHEAT_THRESHOLD) {
        relayState = false;
        safetyOverride = true;
    }

    if (!safetyOverride) {
        if (mode == 0) relayState = false;
        else if (mode == 1) relayState = true;
        else relayState = relayRequested;
    }

    tw_host_write_gpio(PIN_RELAY, relayState ? 1 : 0);

    // LEDs
    const modePatterns: StaticArray<i32> = [LED_OFF, LED_SOLID, LED_BLINK_SLOW];
    tw_host_set_led(PIN_LED_MODE, unchecked(modePatterns[mode]));
    tw_host_set_led(PIN_LED_RELAY, relayState ? LED_SOLID : LED_OFF);
}

export function applet_on_command(cmdType: i32, dataPtr: usize, dataLen: i32): void {
    // Handle mode change from hub
    if (cmdType == 0x80 && dataLen >= 1) {
        const newMode = load<u8>(dataPtr);
        if (newMode <= 2) {
            mode = newMode;
            tw_host_config_set_i32_s(changetype<usize>(modeKey), modeKey.byteLength, mode);
        }
    }
}
