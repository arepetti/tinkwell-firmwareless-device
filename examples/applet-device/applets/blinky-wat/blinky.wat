;; blinky.wat -- Blinky applet in hand-written WebAssembly Text Format.
;;
;; This is the lowest-level way to write an applet.  Every byte is
;; visible.  Useful for understanding the raw WASM ABI that all other
;; languages compile down to.
;;
;; Assemble with: wat2wasm blinky.wat -o blinky.wasm
;;
;; SPDX-License-Identifier: MIT

(module
  ;; --- Imports from the device runtime ---

  (import "env" "tw_host_write_gpio"
    (func $write_gpio (param i32 i32)))

  ;; tw_host_log_s: (msg_ptr: i32, msg_len: i32, level: i32)
  (import "env" "tw_host_log_s"
    (func $log (param i32 i32 i32)))

  ;; --- Memory (for the log string) ---

  (memory (export "memory") 1)     ;; 1 page = 64 KB

  ;; "blinky-wat: init" (16 bytes, no NUL terminator needed)
  (data (i32.const 0) "blinky-wat: init")

  ;; --- Mutable global: LED state ---

  (global $state (mut i32) (i32.const 0))

  ;; --- Exported functions ---

  (func (export "applet_init")
    ;; Log with explicit pointer and length, INFO level (2)
    (call $log (i32.const 0) (i32.const 16) (i32.const 2))
  )

  (func (export "applet_tick")
    ;; state = !state
    (global.set $state
      (i32.xor (global.get $state) (i32.const 1)))

    ;; tw_host_write_gpio(1, state)
    (call $write_gpio
      (i32.const 1)           ;; PIN_LED
      (global.get $state))
  )

  (func (export "applet_on_command") (param i32 i32 i32)
    ;; No commands handled.  Parameters: type, data_ptr, len
  )
)
