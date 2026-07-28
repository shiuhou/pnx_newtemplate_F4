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

Core `app_start`:
`demo/cboard/board_smoke/app.cpp`

USB `app_start`:
`demo/cboard/usb_cdc/app.cpp`

## Generated-code ownership

`boards/dji_c_board_f407/dji_c_board_f407.ioc` and the generated F407 tree are
CubeMX-owned. Do not hand-edit generated hardware mappings or regenerate them
without a separate, reviewed CubeMX task.

## Prerequisites

- CMake 3.22 or newer
- Ninja
- GNU Arm Embedded toolchain providing `arm-none-eabi-gcc` and
  `arm-none-eabi-g++`
