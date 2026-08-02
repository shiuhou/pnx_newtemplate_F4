# DJI C-board STM32F407 ownership

> 中文定位：這裡是「車輛程式需要下探到 F407 時」才閱讀的 board 層。先由根 README
> 進入 MyCar，只有追查 CAN1、USART3、ThreadX 啟動或 HAL callback 時才依下表開啟對應
> `bsp/bsp_*.cpp`。不要從應用程式直接包含本資料夾的 HAL handle、腳位或 IRQ 名稱。

This directory is the only F407/board-specific layer. Public contracts remain
in `pnx_bsp`; STM32 HAL handles, pins, registers, DMA channels and IRQ details
must not leave this directory.

Each `bsp/bsp_*.cpp` directly defines the corresponding public `bsp::*`
symbols. There is no separate `detail::backend_*` forwarding layer.

| Resource | Authoritative owner | Lifecycle |
| --- | --- | --- |
| Clock, startup, linker, ThreadX Cortex-M4 | CubeMX/CMSIS files in this directory | Always present |
| Indicator PH10/PH11 and diagnostics/fault record | `bsp/bsp_indicator.cpp`, `bsp/bsp_diagnostics.cpp`, `bsp/fault_handlers.S` | Core |
| USB OTG FS / USBX | IOC/generated sources + `bsp/bsp_usb.cpp` | Only the USB closure |
| Servo C2, TIM1_CH2/PE11 | `bsp/bsp_pwm.cpp` | Only the attended PWM closure; compare is cleared on stop |
| CAN1/CAN2 bxCAN | IOC/generated handles + `bsp/bsp_can.cpp` | Direct implementation retained; no active application closure |
| USART1/3/6 | IOC/generated handles + `bsp/bsp_usart.cpp` | Direct implementation retained; no active DBUS closure |
| SPI1, PA7/PB3/PB4, Mode 3 | `bsp/bsp_spi.cpp` | Direct implementation retained; no active BMI088 closure |
| Flash geometry and erase/program | `bsp/bsp_flash.cpp` | `UNSUPPORTED_UNTIL_RESERVED_PARTITION`; not linked into a supported RC2 image and no destructive hardware test |

Generated `can.c` or `usart.c` establishes hardware capability and HAL handles;
it does not start the public BSP service. Root CMake decides whether a direct
BSP source belongs to the current firmware image and derives the matching
`main.c` init guard from that same source list. If the authority macros are
missing, `main.c` fails compilation instead of silently skipping ownership.

## Public consumer boundary

Application, Device and Module code may include the board-neutral headers and
call APIs such as `bsp::can::transmit()` or
`bsp::usart::start_rx_to_idle()`. They must not include this directory or name:

- `hcan1`, `hcan2`, `huart1`, `huart3`, `huart6`;
- `HAL_CAN_*`, `HAL_UART_*`, GPIO ports or pin numbers;
- DMA stream/channel or IRQ names;
- F407 register/peripheral instances.

That boundary lets other boards expose the same public contract without
placing their implementation in this F407 repository.

## Manual resource ownership

SPI1 and TIM1 are deliberately manual resources and are not IOC peripherals.
The IOC owns the BMI088 chip-select safe-high boot state; `bsp_spi.cpp` may
reassert the same safe polarity before enabling SPI1. This is a lifecycle
handoff, not two competing configurations.

There must always be one owner per peripheral. Application code must not
initialize SPI1, TIM1 or the same GPIO pins again.

## ThreadX composition boundary

`app_start()` is called by `tx_application_define()` before ThreadX starts
scheduling application threads. It may initialize peripherals, create static
resources and create auto-start threads. It must not call a ThreadX API that
can wait or suspend, such as `tx_mutex_get(..., TX_WAIT_FOREVER)`,
`tx_semaphore_get` or `tx_thread_sleep`; place that work in the thread entry
function.

USART RX and notify handlers execute from UART/DMA ISR context and must remain
bounded and non-blocking. CAN RX handlers also execute from ISR context.
The USART direct source publishes RX metadata before enabling DMA reception
and rolls it back on HAL startup failure. The USB direct source serializes
CDC instance and TX-completion lifecycle state; late callbacks from a retired
connection are ignored.

## USB lifecycle and callback context

`bsp::usb::init()` is an asynchronous startup boundary. `ok` means that the
configuration and callback ownership were accepted, the required ThreadX
resources were created, and the startup worker was scheduled. It does not mean
that USB has enumerated, a host is connected, or CDC is ready. Repeating the
identical configuration is idempotent; changing callback pointers, user
context, priorities, period, or transfer limits is rejected.

`bsp::usb::connected()` becomes true only after the controller is running and
the USBX CDC ACM transport has activated. Controller-start failures enter the
observable `fault` link state. The earlier CubeMX PCD initialization runs
before the BSP lifecycle exists; its existing failure path is the diagnostic
fail-stop `Error_Handler()`.

The raw callbacks execute as follows:

- `fill_tx` runs in the BSP USB worker thread;
- `on_rx` runs in the USBX CDC bulk-OUT ThreadX thread;
- `on_tx_done` runs in the USBX CDC bulk-IN ThreadX thread;
- no raw user callback is invoked directly from an interrupt.

Callback buffers are valid only for the duration of the call. Callbacks must
remain bounded and non-blocking. Disconnect immediately makes
`connected()`/`write()` fail closed, cancels queued or in-flight ownership,
and prevents a late completion from reporting success for the retired
transfer. `write()` copies caller data into a bounded queue and never waits
indefinitely or accumulates an unbounded backlog.

This disconnect guarantee also relies on the pinned USBX 6.1.10 ordering:
CDC ACM deactivation aborts endpoint transfers and executes
`TRANSMISSION_STOP` (suspending the bulk callback threads) before invoking
`usb_cdc_deactivate()`. The board generation check starts at that quiescence
point and remains valid when USBX reuses the same CDC class-instance pointer.

## CubeMX regeneration rule

Regenerate only after reviewing both the IOC diff and generated-source diff.
Keep manual SPI1/TIM1 pins unclaimed by the IOC. If either peripheral moves to
CubeMX ownership, remove the corresponding manual initialization first, then
rerun all retained builds, host tests and the relevant attended hardware
validation.

Do not treat the removal of BMI088/DBUS/CAN-M2006 validation closures as proof
that these direct implementations work on current hardware. Their current
software evidence is limited to public host contracts and ARM syntax checks;
historical hardware observations remain in the root `HANDOFF.md`.
