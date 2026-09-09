# Evidence

- Base: `mycar_f4@b551345`.
- Verified BSP dependency: `pnx_bsp@0f5ef5f`.
- Verified remoter dependency: `pnx_modules@3009a42`.
- Generated configuration: `ENABLE_PS2=1`, `ENABLE_PS2_UART=0`,
  `PNX_PS2_BACKEND_SPI=1`, `PNX_PS2_BACKEND_GPIO=0`.
- Product build: `cmake --preset f407-mycar-combined-ps2-debug` then
  `cmake --build --preset f407-mycar-combined-ps2-debug -j 8` passed.
- ELF SHA-256: `66F0D0E9EABD5C91E36CC0491EEE217853FE628883306807AAA9E7E8D5B61D97`.
- Host validation: 60/60 CTest passed, including the PS2 protocol and unchanged
  combined control contracts.
- Compile commands prove `PNX_F407_PS2_RECEIVER_SAFE_PCLK1=1` reaches both
  `main.c` and `can.c`.
