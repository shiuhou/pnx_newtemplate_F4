# Pure-F407 Repository Rules

This repository contains only the minimal DJI C-board STM32F407 product
graphs: Core F407 and Core F407 plus USB CDC.

- Use `pnx_template@c025ad41b370faaeab128cf6389963a12e154a68` as the
  shared-architecture baseline.
- Keep F407 compiler, generated, Board, startup, linker and ThreadX details
  below the existing Board/platform boundary.
- Do not add lab, motor-test, sensor-draft, release, promotion or hardware
  evidence architecture to the normal product graph.
- Motor output remains zero; non-zero output and all hardware operation
  require separate explicit authorization.
- USB identity remains unassigned until explicitly approved, and controller
  start must remain fail-closed.
- Do not push, modify remotes, tag, publish, regenerate CubeMX or operate
  hardware without explicit authorization.
