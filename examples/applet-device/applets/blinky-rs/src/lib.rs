//! Blinky applet in Rust.
//!
//! Compiled with:
//!   cargo build --target wasm32-unknown-unknown --release
//!   cp target/wasm32-unknown-unknown/release/blinky.wasm ../build/

#![no_std]

extern "C" {
    fn tw_host_write_gpio(pin: i32, value: i32);
    fn tw_host_log_s(msg: *const u8, msg_len: u32, level: i32);
}

static mut STATE: bool = false;
const PIN_LED: i32 = 1;

#[no_mangle]
pub extern "C" fn applet_init() {
    unsafe {
        let msg = b"blinky-rs: init";
        tw_host_log_s(msg.as_ptr(), msg.len() as u32, 2); // INFO
    }
}

#[no_mangle]
pub extern "C" fn applet_tick() {
    unsafe {
        STATE = !STATE;
        tw_host_write_gpio(PIN_LED, if STATE { 1 } else { 0 });
    }
}

#[no_mangle]
pub extern "C" fn applet_on_command(_cmd_type: i32, _data: *const u8, _len: i32) {
    // No commands handled in blinky.
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
