# Brief

Replace only the physical PS2 input path in the MyCar combined product:
UART PS2 becomes synchronous SPI2 PS2. Preserve the existing chassis, arm,
manual/vision-auto arbitration, button mapping, and safety behavior.

Success means the product selects `bindings.ps2_backend=spi`, injects a
`remoter::ps2` source into the existing remoter service, polls at the shared
10 ms cadence, rejects controller ID `0x41`, builds for F407, and retains CAN
at 1 Mbps under the receiver-safe APB1 clock.
