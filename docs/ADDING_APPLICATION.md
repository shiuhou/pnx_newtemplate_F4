# Adding an F407 Application

Use an existing application under `demo/cboard/` as the pattern and keep
startup ownership in `app_start()`.

1. Add a board-specific application directory.
2. Keep reusable hardware access in `pnx_bsp`, device behavior in
   `pnx_devices`, and continuous services in `pnx_modules`.
3. Add a fixed application value to the F407-only CMake list.
4. Add a dedicated preset with its own build directory.
5. Preserve descriptor fail-closed and motor-zero defaults.
6. Run host, CubeMX, boundary, ELF/MAP, and no-H7 delivery checks.

If the change alters a shared contract, implement and regress it first in
`../../pnx_h7_f4`, then promote a new accepted export.
