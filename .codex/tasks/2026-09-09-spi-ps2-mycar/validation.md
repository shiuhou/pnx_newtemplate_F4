# Validation

## Completed

- Embedded F407 Debug configure and link.
- Host suite: 60/60 pass.
- Protocol contract: 9-byte poll query, valid analog frame decode, fail-closed
  invalid handling, and explicit `0x41` rejection.
- Product contract: SPI backend selected, UART backend disabled, USART6 vision
  binding preserved.
- Source diff: no change to `ps2_input_adapter.cpp` or `mode_router.cpp`.
- Clock contract: 168 MHz core, 21 MHz APB1, SPI2 `/256`; CAN timing becomes
  prescaler 1 with 14+6 TQ, preserving 1 Mbps.
- Flashed through the existing OpenOCD CMSIS-DAP server. GDB loaded 109,944
  bytes, every ELF section reported `matched`, then reset the target to run.

## Pending attended hardware checks

- Confirm live PS2 frames and disconnect fail-closed behavior.
- Support the chassis, then confirm manual translation/yaw, arm controls, and
  vision-auto entry/exit with the existing mapping.
