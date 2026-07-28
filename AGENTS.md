# Pure-F407 Repository Rules

This repository contains the DJI C-board STM32F407 architecture release
candidate. Its only product graphs are Core and optional USB CDC. PWM A2,
BMI088, DBUS RX, and CAN/M2006 are isolated validation closures, not product
profiles.

- Use `pnx_template@c025ad41b370faaeab128cf6389963a12e154a68` as the
  shared-architecture baseline.
- Keep F407 compiler, generated code, Board, startup, linker, ThreadX, HAL,
  and resource mapping below the Board/platform boundary.
- Keep shared public APIs free of STM32 HAL types, MCU-family symbols, H7
  memory-bank labels, and board handle names.
- IOC/CubeMX owns generated resources. The documented manual board resources
  are SPI1/BMI088 and TIM1_CH2/PE11 PWM; do not transfer ownership without a
  reviewed CubeMX task and hardware revalidation.
- Core must not acquire optional USB/CAN/PWM/SPI/BMI088/DBUS/Flash closures.
- Motor output remains zero in normal images. Non-zero motor output and all
  hardware operation require separate explicit authorization.
- USB identity remains unassigned until explicitly approved, and controller
  start must remain fail-closed.
- Do not push, modify remotes, tag, publish, regenerate CubeMX, or operate
  hardware without explicit authorization.
