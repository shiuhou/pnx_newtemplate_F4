# Current task

- Objective: replace the MyCar combined product's UART PS2 receiver with the
  verified SPI2 synchronous PS2 backend without changing operator controls.
- Branch: `feat/spi-ps2-mycar`, based on `mycar_f4@b551345`.
- State: implementation and software verification complete; hardware flash and
  attended vehicle-motion validation remain pending.
- Verified: embedded Debug build passes; Host CTest passes 60/60; generated
  config selects SPI PS2 and disables UART PS2; control adapter/router have no
  diff from the baseline.
- Dependencies: local pins `pnx_bsp@0f5ef5f` and `pnx_modules@3009a42` are not
  yet advertised by their configured upstream remotes, so publish/promote them
  before publishing the parent branch.
- Safety: APB1 is 21 MHz for this image; CAN1/CAN2 timing is adjusted to remain
  at 1 Mbps. No flash or physical motion was performed in this task.
