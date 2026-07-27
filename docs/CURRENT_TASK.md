# Current Task

Prepare the pre-sensor-integration checkpoint without building, testing or
operating hardware.

## Current boundary

- Six F407 preset builds: `RETAINED_PASS`; exact current combined-tree
  regression: `NOT_RUN`.
- Previous host suite: `RETAINED_34_OF_34_PASS`; expanded current suite:
  `NOT_RUN`.
- USB CDC: `ISOLATED_LAB_PASS`; production identity remains unassigned.
- CAN1 and bounded M2006 pulse: `PASS`.
- PWM C2: `BOUNDED_PHYSICAL_OBSERVATION_PASS`.
- BMI088 replacement-board and IST8310 individual labs: `LAB_PASS`.
- Formal BMI088 + IST8310 common runtime: `NOT_DONE`.
- UART: `SKIPPED_BY_USER_NOT_BLOCKING`; CAN2: `NOT_RUN_OPTIONAL`.
- DBUS: `NOT_IMPLEMENTED_NOT_RUN`; MaixCam: `NOT_RUN`; AHRS:
  `OUT_OF_SCOPE`.
- Daily integrated firmware: `PARTIAL`; team publication: `NOT_PUBLISHED`.

The next action after checkpoint acceptance is the exact current combined-tree
software regression. Formal sensor integration remains a separate task.
