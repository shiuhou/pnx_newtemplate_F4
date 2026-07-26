# Pure-F407 Validation

Fresh local software validation completed on 2026-07-27. Hardware was not
used. The machine-readable record is
[release/pnx-f4-local-validation.json](../release/pnx-f4-local-validation.json).

## Result ledger

```text
PURE_F407_LOCAL_BUILD=PASS
PURE_F407_HOST_TESTS=PASS_34_OF_34
PURE_F407_CUBEMX_GATE=PASS
PURE_F407_BOUNDARY=PASS
PURE_F407_NO_H723=PASS
PURE_F407_ELF_MAP_GATE=PASS
PURE_F407_ARTIFACTS_READY=PASS
PURE_F407_DESCRIPTOR_FAIL_CLOSED=PASS
PURE_F407_USB_CONTROLLER_STOPPED=PASS
PURE_F407_MOTOR_DEFAULT_ZERO=PASS
PURE_F407_USB_CCMRAM=0
PURE_F407_HW1=NOT_RUN
F407_CORE_RUNTIME=NOT_RUN
F407_USB_HARDWARE=HARDWARE_UNVERIFIED
```

## Firmware artifacts

Memory values are the linker `--print-memory-usage` results. The combined
fresh build contained zero compiler or linker warnings.

| Preset | Output directory | ELF SHA-256 | Flash | RAM | CCMRAM |
|---|---|---|---:|---:|---:|
| `board-smoke` | `build/dji-c-board` | `1C8C9A0CF5C74DCD37A02346E43A942933701EA495AE7CB6C4106DA07A7CE192` | 44504 B | 52968 B | 0 B |
| `can-rx` | `build/dji-c-board-can-rx` | `043CF9251A2FD27962F6EF4CD135E6DEF21A926F11A87638DEA6DB23293C3F97` | 70432 B | 60832 B | 0 B |
| `motor-safe` | `build/dji-c-board-motor-safe` | `F5CD83A612389895341DB49BF9F048F2E9097695C5262F0EFC8C12C1C0C95604` | 71080 B | 60856 B | 0 B |
| `board-smoke-release` | `build/dji-c-board-release` | `D933CE7A22AA619C54E55200AA2C1FA4589291C642D7F02F5F86E52F9EF90573` | 25920 B | 52968 B | 0 B |
| `usb-cdc` | `build/dji-c-board-usb-cdc` | `C6B4F863F60EEDA918711D41142CCB1C59012E6671979EB0CD0F793601141FF7` | 85200 B | 71072 B | 0 B |
| `usb-cdc-release` | `build/dji-c-board-usb-cdc-release` | `DF21F2073317355066624C0DA5B56C727EE0BCB8E25C98D5264CEDE62859D190` | 50076 B | 70976 B | 0 B |

Every build directory contains ELF, HEX, BIN, MAP, and the actual-target
source inventory. Every ELF is little-endian ARM ELF32 and contains
`Reset_Handler`, `main`, `tx_application_define`, `app_start`, and
`HardFault_Handler`. No H723 or legacy `board/` source entered any build graph.

## Executed checks

| Check | Result |
|---|---|
| two immutable export candidates | both candidate gates `PASS`; Git tree and canonical SHA-256 identical |
| promoted workspace gate | `PASS` |
| CubeMX production gate | `PASS`; six F407 presets and four clean exact submodules |
| board boundary gate | `PASS` |
| F407 USB source acceptance | `PASS` (9/9) |
| host CTest | `PASS` (34/34) |
| descriptor identity | unconfirmed, VID/PID `0x0000`, serial `UNASSIGNED`; `PASS` |
| fail-closed USB link | `bsp::usb::init` absent from both USB images; controller remains stopped |
| motor-safe configuration | non-zero test disabled; bus/model `none`, ID/current `0`; guard test `PASS` |
| JSON, Markdown, licence, credential, temporary-file and tracked absolute-path checks | `PASS` |

Current workspace commands:

```powershell
python -B scripts/check_f407_only.py --repo-root . --promoted-workspace
python -B scripts/check_cubemx_production.py --repo-root . --check-submodules-clean
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_dji_c_board.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_host.ps1
```

The default candidate-mode checker applies only to the two immutable
deterministic export directories. It intentionally does not accept the
promoted documentation/configuration overlay. The immutable
`release/f407-only-provenance.json` remains an F4-2 candidate snapshot with its
original `NOT_RUN` values; current results are recorded in this document and
the local validation manifest.
