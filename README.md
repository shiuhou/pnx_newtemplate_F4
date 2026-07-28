Core F407:

cmake --preset f407-debug
cmake --build --preset f407-debug

USB CDC:

cmake --preset f407-usb-cdc-debug
cmake --build --preset f407-usb-cdc-debug

# PnX DJI C-board F407

This repository is the `pnx_template` architecture reduced to the minimum
STM32F407 port for the DJI C-board. It contains one Core image and one
optional USB CDC image.

## Architecture boundary

Shared PnX interfaces remain in the `pnx_*` submodules. STM32F407 compiler
flags, CMSIS/HAL, startup, linker, ThreadX Cortex-M4 port, generated sources
and resource bindings stay under `boards/dji_c_board_f407`, `configs` and
`cmake`.

## Targets

Core and USB use the same F407 startup, linker, clock, GPIO, ThreadX bootstrap
and Board base. The USB target adds only USB OTG FS generated code, USBX, CDC
descriptors, the USB backend and the minimal echo application. Its descriptor
identity is unassigned by default, so controller start remains fail-closed.

The single application composition root is `demo/app.cpp`. Core selects
`demo/cboard/board_smoke/board_smoke.cpp`; USB CDC selects
`demo/cboard/usb_cdc/usb_cdc.cpp`.

## Demo source status

MCU-independent demos from `pnx_template` are retained as reference source for
learning and future adaptation. Source presence does not mean that an
application is currently buildable on F407. The template USB demo is
intentionally not restored; the current F407 `usb_cdc` component is the only
USB application.

| Application | Source present | F407 buildable | Current status |
| --- | --- | --- | --- |
| board_smoke | YES | YES | Core application |
| usb_cdc | YES | YES | F407 USB application |
| motor | YES | NO | Awaiting CAN/device compatibility |
| usart | YES | NO | Awaiting minimal USART backend |
| remoter | YES | NO | Awaiting DBUS/USART support |
| referee_ui | YES | NO | Awaiting USART/protocol integration |
| imu | YES | NO | Awaiting F407 sensor integration |

Only `board_smoke` and `usb_cdc` enter the current product build graph.
Restored motor, IMU, remoter, referee and USART demos remain excluded. This
task does not claim new hardware validation; any prior hardware validation
belongs to the frozen baseline and was not repeated here.

## Generated-code ownership

`boards/dji_c_board_f407/dji_c_board_f407.ioc` and the generated F407 tree are
CubeMX-owned. Do not hand-edit generated hardware mappings or regenerate them
without a separate, reviewed CubeMX task.

## Prerequisites

- CMake 3.22 or newer
- Ninja
- GNU Arm Embedded toolchain providing `arm-none-eabi-gcc` and
  `arm-none-eabi-g++`
