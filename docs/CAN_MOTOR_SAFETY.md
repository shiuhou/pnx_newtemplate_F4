# CAN and Motor Safety

The normal `motor-safe` preset compiles with non-zero motor output disabled.
Offline, stale-feedback, CAN/drop, and service-fault paths command zero.

Non-zero output requires a separate bounded bench approval with explicit CAN
bus, feedback ID, motor model, current limit, interlocks, and stop conditions.
Never weaken the compile gate or defaults to make a test pass.

During core bring-up keep CAN, motor power, and PWM disconnected. Passive CAN
receive and motor tests are later hardware gates and must retain logs.
