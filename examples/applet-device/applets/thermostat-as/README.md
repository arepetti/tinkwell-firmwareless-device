# Thermostat Applet (AssemblyScript)

The same thermostat state machine as the compiled C example, rewritten in AssemblyScript and compiled to WASM.
This is pushed to the device at runtime by the hub.

## Build

```bash
npm install
npm run build
```

Produces `build/thermostat.wasm`.

## How it works

The applet imports host functions (`tw_host_read_sensor_sn`, `tw_host_write_gpio`, etc.) and exports `applet_init`, `applet_tick`, and `applet_on_command`.
The device runtime calls these on each cycle.

The logic is identical to the compiled thermostat: OFF/ON/AUTO modes, freeze and overheat safety overrides, LED patterns.
