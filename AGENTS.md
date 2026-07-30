# Pure-F407 Repository Rules

This repository contains the DJI C-board STM32F407 architecture release
candidate. Its only product graphs are Core and optional USB CDC. PWM A2 is
an isolated validation closure, not a product profile. BMI088, DBUS RX,
and CAN/M2006 evidence is historical and is not a build surface after the
Device/Lib/Module gitlinks return to the `pnx_template` baseline.

- Use `pnx_template@c025ad41b370faaeab128cf6389963a12e154a68` as the
  shared-architecture baseline.
- Keep F407 compiler, generated code, Board, startup, linker, ThreadX, HAL,
  and resource mapping below the Board/platform boundary.
- Keep shared public APIs free of STM32 HAL types, MCU-family symbols,
  other-MCU memory-bank labels, and board handle names.
- IOC/CubeMX owns generated resources. The documented manual board resources
  are SPI1/BMI088 and TIM1_CH2/PE11 PWM; do not transfer ownership without a
  reviewed CubeMX task and hardware revalidation.
- Core must not acquire an optional public BSP implementation or consumer
  closure. Generated `can.c`/`usart.c` may remain compiled as dormant Board
  capability, but root CMake must derive their `main.c` init guards from the
  actual Direct BSP source selection; Core must not execute those init calls.
- Add a validation selector, manager, telemetry or test framework, or new
  abstraction only when it is required by an existing `pnx_template`
  contract, a proven F407 hardware difference, or a current product feature.
  If none applies, do not add it.
- Validation closures must not become the product subsystem-selection
  architecture. Production robot applications belong in separate
  vehicle-specific repositories, which compose the accepted shared submodules
  and F407 Board baseline without expanding this repository into a product
  application.
- Motor output remains zero in normal images. Non-zero motor output and all
  hardware operation require separate explicit authorization.
- USB identity remains unassigned until explicitly approved, and controller
  start must remain fail-closed.
- Do not push, modify remotes, tag, publish, regenerate CubeMX, or operate
  hardware without explicit authorization.
