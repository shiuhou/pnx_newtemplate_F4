# USB CDC Boundary

The USB CDC software closure is present, but physical USB remains
`HARDWARE_UNVERIFIED`.

```text
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
USB_CONTROLLER_DEFAULT=STOPPED
USB_ENUMERATION=NOT_RUN
USB_BIDIRECTIONAL_DATA=NOT_RUN
USB_RECONNECT=NOT_RUN
```

Do not assign an arbitrary VID/PID, reuse the ST vendor identity, or enable
controller start without explicit approval. Core runtime validation precedes
USB enumeration.

The `usb-cdc` and `usb-cdc-release` presets build the fail-closed software
path. They do not authorize attaching the USB data port to a host.
