/**
 * Blinky applet in AssemblyScript.
 *
 * Compiled with: npx asc assembly/index.ts -o build/blinky.wasm
 */

@external("env", "tw_host_write_gpio")
declare function tw_host_write_gpio(pin: i32, value: i32): void;

@external("env", "tw_host_log_s")
declare function tw_host_log_s(msg: usize, msg_len: u32, level: i32): void;

const PIN_LED: i32 = 1;
let state: bool = false;

export function applet_init(): void {
    const msg = String.UTF8.encode("blinky-as: init");
    tw_host_log_s(changetype<usize>(msg), msg.byteLength, 2); // INFO
}

export function applet_tick(): void {
    state = !state;
    tw_host_write_gpio(PIN_LED, state ? 1 : 0);
}

export function applet_on_command(cmdType: i32, dataPtr: usize, dataLen: i32): void {
    // No commands handled in blinky.
}
