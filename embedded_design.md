# Task E - Embedded integration design (design + SIMULATED mocks; nothing here touches real hardware)

## Block diagram
```
                      Windows 11 host (Workbench: Launch.exe -> 127.0.0.1 UI)
                        |  USB 2.0 (powered hub, 5 V, per-port current limit)
        +---------------+------------------+--------------------+
        |                                  |                    |
  [USB-UART CDC]                     [USB-UART CDC]        [USB (vendor protocol)]
        |                                  |                    |
+-------v--------+   UART/SPI 3.3 V  +-----v----------+   +-----v-----------+
| MCU board      |<----------------->| FPGA board     |   | JTAG adapter    |
| 3.3 V I/O      |   (+ common GND)  | Vcore 1.0 V    |   | TCK TMS TDI TDO |
| SWD header     |                   | VCCIO 3.3 V    |<--| VTref, GND,     |
+----------------+                   | JTAG header    |   | (nTRST/nSRST)   |
                                     +----------------+   +-----------------+
 JTAG adapter = debug/config interface to the FPGA TAP (and to an MCU only if that MCU exposes JTAG; most Cortex-M parts use SWD instead).
```
## Interface / voltage table
| Link | Signals | Level / domain | Direction & ownership | Notes / assumptions |
|---|---|---|---|---|
| Host - MCU | USB D+/D-, VBUS, GND | USB 5 V, MCU I/O 3.3 V | host powers; MCU is device | on-board USB-UART bridge (CDC) assumed |
| Host - FPGA | USB D+/D-, VBUS, GND | USB 5 V, core 1.0 V, VCCIO 3.3 V (assumed) | host powers; FPGA board regulators derive rails | verify bank voltage in board manual |
| Host - JTAG adapter | USB | USB 5 V | adapter is USB device | vendor driver/protocol, not CDC |
| JTAG adapter - FPGA | TCK, TMS, TDI (adapter drives); TDO (target drives); VTref, GND; optional nTRST/nSRST | adapter buffers track **VTref** (1.8-3.3 V) | VTref is an *input sense* from the target; adapter must not power the target | pin order differs per board: use the board's JTAG header pinout, never assume |
| MCU <-> FPGA | UART TX/RX (or SPI), GND | both 3.3 V in this design -> direct; if FPGA bank is 1.8 V use a level shifter (e.g. 74AVC/TXB class) | each end owns its TX; common ground mandatory | no 5 V tolerance assumed on either side |
| Power | per-board USB or own supply | do not parallel supplies; no back-feeding through signal pins | | single ground reference star at the hub |
## Why a JTAG adapter is not a generic compute board
It is a protocol bridge: the host sends TAP state-machine sequences (TMS/TCK, shifts through IR/DR) and the adapter replays them on four wires with level translation to the target's VTref. It runs no user application, has no program memory the user targets, and exposes no general I/O. The MCU and FPGA are *targets*; the adapter is the *probe*. Boards differ in connectors (2x5 1.27 mm, 2x7 2.54 mm, Tag-Connect), voltage (1.8/2.5/3.3 V), and protocol (JTAG vs SWD), so nothing here is interchangeable.
## Mock adapters (C++, `src/core/devices.*`)
`discover`, capability list, `connect/disconnect`, and one telemetry message per device (MCU: vdd/die temp/uptime; FPGA: vccint/vccio/DONE/temp; JTAG: VTref/TCK/chain IDCODE). **All values are simulated and labelled SIMULATED in API and UI.** Recoverable failure: the first FPGA connect returns `TIMEOUT` (injected); retry succeeds. Tested in `tests/test_core.cpp`.
## Safe integration plan
1. Read each board's manual: record VCCIO, header pinout, 5 V tolerance. 2. Power each board alone, measure rails with a meter before connecting anything. 3. Connect UART with GND first, TX/RX cross-wired, logic-analyser check of idle levels. 4. Connect JTAG with the target powered; confirm VTref, then scan the chain read-only (IDCODE) before any write. 5. Only then enable configuration/programming; keep a current-limited supply.
## Limitations
Pin numbers, voltages and header types are assumptions to be replaced with the chosen boards' datasheets; no physical validation, no firmware, no real programming.
