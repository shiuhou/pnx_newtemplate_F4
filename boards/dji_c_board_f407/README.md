# DJI C-board STM32F407 Board Ownership

This directory is the only MCU/board-specific layer. Public contracts remain
in the `pnx_*` submodules; STM32 HAL handles and register access must not leave
this directory.

| Resource | Authoritative owner | Lifecycle |
| --- | --- | --- |
| Clock, startup, linker, ThreadX Cortex-M4 | CubeMX/CMSIS files in this directory | Always present |
| CAN1/CAN2, USART1/3/6, USB OTG FS | IOC + generated Core sources | Hardware capability; a BSP service starts only in its selected closure |
| BMI088 CS PA4/PB0 safe-high boot state | IOC + generated `MX_GPIO_Init()` | Configured before ThreadX |
| BMI088 SPI1, PA7/PB3/PB4, Mode 3 | `pnx_backends/spi_backend.cpp` | Initialized only by the BMI088 closure after `app_start()` |
| Servo C2, TIM1_CH2/PE11 | `pnx_backends/pwm_backend.cpp` | Initialized only by the attended PWM closure; compare is cleared on stop |
| Flash geometry and erase/program | `pnx_backends/flash_backend.cpp` | Not linked into Core; no destructive hardware test is defined |

SPI1 and TIM1 are deliberately manual resources: they are not IOC peripherals
and must stay absent from generated ownership until an explicit migration is
reviewed. The SPI backend may reassert the IOC-defined BMI088 chip-select safe
state before enabling SPI1; this is a same-polarity handoff, not a second
configuration source.

## ThreadX composition boundary

`app_start()` is called by `tx_application_define()` before ThreadX starts
scheduling application threads. It may initialize peripherals, create static
resources, and create auto-start threads. It must not call a ThreadX API that
can wait or suspend, such as `tx_mutex_get(..., TX_WAIT_FOREVER)`,
`tx_semaphore_get`, or `tx_thread_sleep`; place that work in the thread entry
function instead. This keeps composition deterministic before a current
application thread exists.

## CubeMX regeneration rule

Regenerate only after reviewing the IOC diff and generated-source diff. Keep
manual SPI1/TIM1 pins unclaimed by the IOC. If a future change moves either
resource into CubeMX, remove or replace the corresponding backend ownership
first, then rerun all builds/host tests and its attended hardware validation.
