# RFID UID Hardware Debug Guide

This guide is for the first attended hardware test of the M3650A-HA reader on
the DJI RoboMaster Development Board Type C. It validates UART communication
and UID reporting only. It does not implement station or difficulty logic.

## Source and firmware

- Git branch: `feat/rfid`
- Implementation commit: `27ee62a6b1190d328215d62b400c0fc2732385cd`
- Zero-motor preset: `f407-rfid-uid-debug`
- RFID UART: STM32 USART6, 9600 baud, 8 data bits, no parity, 1 stop bit
- Debug UART: STM32 USART1, 115200 baud, 8 data bits, no parity, 1 stop bit

Build products under `build/` are intentionally ignored by Git. The source,
configuration, tests, and build preset are on GitHub, but the ELF is not.

## Get and build the firmware

From a fresh checkout:

```powershell
git clone --recurse-submodules https://github.com/shiuhou/pnx_newtemplate_F4.git
cd pnx_newtemplate_F4
git switch feat/rfid
git submodule update --init --recursive
cmake --preset f407-rfid-uid-debug
cmake --build --preset f407-rfid-uid-debug --parallel 4
```

The resulting image is:

```text
build/f407-rfid-uid-debug/pnx_embedded.elf
```

The already-built local ELF on the original development machine is:

```text
C:\Users\USER\Desktop\RM\rm_inschool\2026\firmware\pnx_f4_rfid\build\f407-rfid-uid-debug\pnx_embedded.elf
```

Its observed file size is `1,617,384` bytes and SHA-256 is:

```text
8F6998958AE0029EF4C6EA8ADCFC8B46014ADFB4D01B3E674FB778FE55898313
```

Rebuilding from commit `27ee62a` is preferred because it keeps the binary
traceable to source and avoids passing an untracked build artifact around.

## Electrical check before connection

The vendor specification lists both TTL UART and RS232 variants. A 3.3-5 V
supply rating does not identify the signal interface.

1. Disconnect motors and CAN devices. Do not use the combined vehicle image.
2. With RFID TX/RX disconnected from the C-board, power only RFID VIN and GND.
3. Measure RFID TX relative to GND. A negative idle voltage indicates RS232;
   do not connect it directly to the STM32. Use an RS232-to-TTL transceiver.
4. Continue only after confirming the reader is the TTL UART variant and its
   signal levels are compatible with the C-board.
5. Make every wiring change with power off and use a common ground.

The module supply is documented as 3.3-5 V DC and less than 75 mA. A
current-limited bench supply is preferred for the first test.

## RFID wiring

On the photographed M3650A-HA, connector J1 is labelled:

```text
IO2  IO1  TX  RX  GND  VIN
```

Leave IO1 and IO2 disconnected.

The C-board shell label is misleading: shell `UART1` is the STM32 USART6
3-pin connector used by RFID. Locate Pin 1 from the PCB/manual marking rather
than guessing its physical left/right orientation.

| M3650A-HA | C-board shell `UART1` / STM32 USART6 |
| --- | --- |
| TX | Pin 3, RXD |
| RX | Pin 2, TXD |
| GND | Pin 1, GND |
| VIN | Regulated 3.3 V or 5 V supply |
| IO1, IO2 | Not connected |

TX and RX must be crossed.

## PC debug UART

The C-board shell `UART2` is the STM32 USART1 4-pin connector used for text
output.

| C-board shell `UART2` | USB-to-TTL adapter |
| --- | --- |
| Pin 2, TXD | RXD |
| Pin 3, GND | GND |
| Pin 1, RXD | Not required |
| Pin 4, 5 V | Do not connect to adapter power |

Do not power the C-board from the USB-to-TTL adapter. Configure the serial
terminal for `115200 8N1`, no flow control.

## Program the zero-motor image

With the existing CMSIS-DAP probe and OpenOCD installation:

```powershell
& "D:\OpenOCD\bin\openocd.exe" `
  -s "D:\OpenOCD\share\openocd\scripts" `
  -f "interface/cmsis-dap.cfg" `
  -f "target/stm32f4x.cfg" `
  -c "adapter speed 2000" `
  -c "program build/f407-rfid-uid-debug/pnx_embedded.elf verify reset exit"
```

Require `Programming Finished`, `Verified OK`, and a target reset. Open the
115200 serial terminal before pressing the C-board reset button because the
firmware reports only state or counter changes, not a continuous heartbeat.

## Expected output

A normal startup progresses through initializing, verifying, and ready:

```text
RFID link=1 event=0 uid=00000000 err=0/0/0/0
RFID link=2 event=0 uid=00000000 err=0/0/0/0
RFID link=3 event=0 uid=00000000 err=0/0/0/0
```

Presenting a compatible ISO14443A card should increment `event` and update
the UID:

```text
RFID link=3 event=1 uid=A1B2C3D4 err=0/0/0/0
```

Link values are:

| Value | Meaning |
| ---: | --- |
| 0 | disabled |
| 1 | initializing |
| 2 | verifying |
| 3 | ready |
| 4 | config mismatch |
| 5 | timeout |
| 6 | local UART I/O error |

The four error counters are checksum, frame, ring overflow, and timeout.

## Troubleshooting order

For `link=4`, verify the module with the vendor tool. The firmware expects
address `0x20`, automatic UID mode `2`, active upload value `0`, and read-once
mode `0`. The firmware deliberately queries these settings but never writes
module EEPROM.

For `link=5`, check power, common ground, crossed TX/RX, TTL versus RS232,
9600 8N1, and module address in that order.

For no debug text, check the separate shell `UART2` connection, select the
correct COM port at 115200 8N1, open the terminal, and reset the C-board.

After UID reading works, test module unplug/replug recovery and compare the
reported UID with the vendor tool. Dynamic vehicle passes, adjacent-tag
behavior, latency, and combined-image failure isolation remain hardware
acceptance work and are not yet claimed.
