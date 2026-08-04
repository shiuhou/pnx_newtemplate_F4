# demo/

Application composition sources for this repository. Built closures and
read-only references are intentionally separated.

## Built composition

Exactly one application closure is compiled into an embedded image:

| Source | Selected by | Status |
| --- | --- | --- |
| `cboard/board_smoke/` | default | zero-motor F407 baseline |
| `cboard/usb_cdc/` | `PNX_ENABLE_USB_CDC` | optional product capability |
| `cboard/pwm_a2/` | `PNX_ENABLE_PWM_A2` | attended validation closure |
| `vehicle/mycar.cpp` + `vehicle/chassis/` | `PNX_ENABLE_MYCAR_CHASSIS` | fail-closed MyCar product closure |

`app.cpp` is the single composition root. CMake selects one closure and the
preprocessor checks reject an invalid selection rather than silently building
a different image.

## `_reference/` — not built

Nothing in `_reference/` belongs to any current build graph. These files are
trimmed F407 wiring examples for IMU, motor, remoter, referee UI, and USART
work. They are retained for reading only and do not establish product support.

Rules:

- Do not add a reference path directly to CMake. A supported peripheral needs
  an explicit closure or vehicle composition with its own validation.
- When a real closure supersedes a reference, remove the reference in the same
  reviewed change.
- Do not refactor `_reference/` as though production code depended on it.
