# Pure-F407 Architecture

This repository exposes one board product and one source graph:

```text
F407 IOC/startup/linker/HAL/ThreadX backend
                    ↓
                 pnx_bsp
                    ↓
               pnx_devices
                    ↓
               pnx_modules
                    ↓
           selected application
```

There is no board-selection step for users. Presets select only the
application and build type. Shared modules remain exact gitlinks; shared
contract changes originate in `../../pnx_h7_f4` and return through an accepted
export.

Board-specific ownership includes IOC, startup, linker, HAL, clocks, IRQ, DMA,
memory, and USB backend. Application code depends on stable BSP/device/module
interfaces rather than HAL handles.
