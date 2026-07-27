# USB CDC Boundary

The USB CDC path passed an isolated hardware lab. This does not assign a
production identity or approve a release.

```text
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
USB_CONTROLLER_DEFAULT=STOPPED
USB_LAB_VID_PID=0xCAFE:0xF407
USB_CDC=ISOLATED_LAB_PASS
USB_ENUMERATION=PASS_ISOLATED_LAB
USB_BIDIRECTIONAL_DATA=PASS_ISOLATED_LAB
USB_RECONNECT=PASS_ISOLATED_LAB
USB_PRODUCTION_IDENTITY=NOT_ASSIGNED
USB_RELEASE_APPROVAL=NOT_APPROVED
```

Do not assign an arbitrary VID/PID, reuse the ST vendor identity, or enable
controller start in the default product image without explicit approval.

The `usb-cdc` and `usb-cdc-release` presets build the fail-closed software
path. They do not authorize attaching the USB data port to a host.

## Approved isolated-lab image

The user approved one local, isolated-lab-only VID/PID on 2026-07-27. It is
not an assigned product identity. Its input and firmware artifact remain only
under ignored `build/` content:

```text
VID=0xCAFE
PID=0xF407
```

Artifact:

```text
build/dji-c-board-usb-cdc-lab/pnx_embedded.elf
SHA-256=0AF3F14695F3CD15EB06690CB719C161A1FB7A431E3E40F2F086D94B59676DC2
```

Use the VS Code configuration
`F407 USB CDC Lab Debug (isolated identity)`. Use micro-USB as the sole
board-power source and leave 24 V, CAN, motors, PWM and DBUS disconnected.
The tracked default and release identities remain fail-closed.

The isolated lab subsequently passed enumeration as `VID_CAFE&PID_F407`,
bidirectional CDC transfer, close/reopen and reset re-enumeration. These
results are `ISOLATED_LAB_PASS` only and are not production USB readiness.
