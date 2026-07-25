# DJI C Board STM32F407 profile

This directory is the production board target for `STM32F407IGH6TR`.

## IOC ownership

Two IOC files have deliberately different roles:

- `dji_c_board_f407.ioc` is the only production CubeMX source. It contains
  CAN1/2, USART1/3/6, five UART DMA streams, GPIO safe states, TIM14 HAL tick,
  the ThreadX core, RCC and SWD.
- `reference/dji_c_board_f407_full_reference.ioc` is a hash-locked,
  Phase-1-patched and line-ending-normalized RM26 hardware/history reference.
  It contains many peripherals that are not part of the current product
  closure and is forbidden from the build graph.

DWT is not an IOC peripheral. `pnx_bsp/dwt` enables and validates the
Cortex-M4 CoreSight cycle counter using the board-owned
`Core/Inc/pnx_cmsis_device.h` selector.

## Safe GPIO state

- PH12 red, PH11 green and PH10 blue are active-high and start RESET/off.
- PA4 and PB0 are retained as BMI088 chip selects and start high/inactive.
- PG6 is retained as `IST8310_RSTN` and starts high.
- Removed SPI/I2C/USB/PWM pins remain unassigned. Full JTAG is not enabled;
  PA13/PA14 remain SWDIO/SWCLK.
- No CAN transceiver EN/STB signal has been proven from the available
  repositories. Confirm it from the actual board revision or measurements.

## Generated-tree correspondence

CubeMX 6.17.0-RC5 with `STM32Cube_FW_F4_V1.28.3` generated the minimal IOC
twice in disposable directories outside the formal worktree. Both outputs
contain 296 files and have the same canonical tree SHA256:

```text
9BAA3577F44804A0022CB8FAD28445ECF335DEA40C091BCCB94DCCF881F08195
```

The formal generated-owned tree contains 290 files: the Gen2 production
closure, one hand-owned CMSIS selector and seven reviewed overlays for PnX
ThreadX startup, diagnostics/fault capture, Newlib stubs and SRAM/noinit
linker rules. Exact file hashes and reasons are locked by:

- `cubemx_provenance.json`
- `provenance/minimal_gen1_manifest.json`
- `provenance/minimal_gen2_manifest.json`
- `provenance/minimal_generation_delta.json`
- `provenance/accepted_formal_tree_manifest.json`

Every F407 configure runs the fail-closed gate before config generation and
again against the resolved generated source graph:

```powershell
python scripts/check_cubemx_production.py --check-submodules-clean
cmake --fresh --preset dji-c-board
```

Do not let CubeMX write directly into this directory. Generate in a disposable
copy, compare two consecutive generations, review the semantic delta, then
update the formal tree and provenance together.

## Current status

```text
IOC_VERIFIED
GENERATED_CODE_VERIFIED
A-07=CLOSED_WITH_DEFERRED_EVIDENCE
SOFTWARE_GATE=SOFTWARE_PASS
CUBEMX_6_15_EXACT_REPLAY=DEFERRED_NON_BLOCKING_FOR_HARDWARE_BRINGUP
PRODUCTION_TOOLCHAIN_REPRODUCIBILITY=DEFERRED_PENDING_VERSION_FREEZE
HARDWARE_UNVERIFIED
```

Exact CubeMX 6.15 replay or a formally accepted stable-version freeze remains
required before reproducible release to another machine. It does not block
safe hardware bring-up with the already built artifacts.
