# F407 Hardware Bring-up

No hardware operation is part of the workspace migration.

## HW-1

Use the VS Code configuration `F407 Board Smoke Debug` or the approved
programmer command with:

```text
build/dji-c-board/pnx_embedded.elf
```

Before programming, record the ELF SHA-256. Retain connection, identification,
program, verify, and reset output. This replay establishes
`PURE_F407_HW1`; the previous multiboard result does not.

## HW-2

After HW-1:

1. stop at `main`;
2. continue through `tx_application_define` and `app_start`;
3. run freely and confirm no unexpected fault or reset loop;
4. inspect LED heartbeat, ThreadX state, DWT, and fault/reset counters;
5. only then attach the approved 3.3 V UART receive path.

Keep CAN, motors, PWM, DBUS, and USB enumeration disconnected until their
later gates.
