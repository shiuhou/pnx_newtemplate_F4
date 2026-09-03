# Current task

- Objective: integrate M3650A-HA UID reporting as an isolated RFID module on
  `feat/rfid`, without adding station logic or affecting chassis/ARM control.
- Baseline: `fe59062856a57121f533375763e0381c7533eeeb`; Host baseline passed
  58/58 before RFID changes.
- Worktree: `C:\Users\USER\Desktop\RM\rm_inschool\2026\firmware\pnx_f4_rfid`.
- Scope guard during implementation: no hardware operation, EEPROM write, M1
  block access, UID deduplication, or motor-path control.

## Vendor protocol facts

Primary source: `RFID读写器二次开发资料V23C(串口)\RFID读写二次开资料V23B(串口)\高频读写器使用手册V1.0.5.pdf`.

- Default link is address `0x20`, 9600 baud, 8 data bits, no parity, 1 stop
  bit. Non-RS485 readers should retain address `0x20`.
- Host command: `type, length, command, address, params/data, checksum`.
  Response adds status at byte 4; status 0 is success.
- Checksum is XOR of bytes `0..length-2`, then bitwise NOT.
- B1 query: `02 08 B1 20 00 00 00 64`.
- Expected B1 response for active automatic UID reporting:
  `02 09 B1 20 00 02 00 00 67` (`mode=2`, ignored block 0,
  `upload=0` active).
- B8 query and expected read-once response:
  `02 08 B8 20 00 00 00 6D` (`mode=0`). Read-once behavior is a module
  property, not a software promise that one physical presentation produces
  exactly one event.
- Automatic UID report:
  `04 0C 02 20 00 04 00 45 96 B7 8A 3F`; card type is bytes 5-6 and UID is
  bytes 7-10.
- Multiple commands should be spaced by more than 100 ms. The implementation
  uses 120 ms, correcting the original plan's exact 100 ms interval.
- The manual's five-second power-on warning applies to setting commands that
  write configuration. This driver sends only B1/B8 queries and never writes
  module EEPROM.
- Relevant documented frames are at most `0x1C` bytes, so the parser's
  8-32-byte bounds cover the vendor protocol while bounding malformed input.

Hardware source: `M3650A-HA资料\M3650A-HA资料\M3650A-HA V1.0规格书.pdf`.

- Module supply is documented as 3.3-5 V DC; communication variants include
  TTL UART and RS232. Supply range does not prove MCU-safe UART levels, so the
  exact variant and signal levels still require hardware verification.

Reference implementation sources:

- `参考代码\2.STM32实例代码\STM32实例代码说明.pdf`
- `参考代码\2.STM32实例代码\STM32读卡(读写器自动读卡号)\User\main.c`
- `参考代码\2.STM32实例代码\STM32读卡(读写器自动读卡号)\User\Uart.c`
- `参考代码\2.STM32实例代码\STM32读卡(读写器自动读卡号)\Lib\stm32f10x_it.c`
- `参考代码\2.STM32实例代码\STM32读卡号读写数据代码示例3.0(C语言)\User\main.c`
- `参考代码\4.C++实例代码\1.读卡号\ConsoleApplication1\ConsoleApplication1.cpp`
- `参考代码\3.C#实例代码\HF接收卡信息10进制显示\thread\Form1.cs`

These examples confirm 9600 8N1, XOR-NOT checksum, status handling, and UID
byte placement. They use delay/global-buffer parsing and contain sample-code
defects, so only protocol behavior is retained; their architecture is not
copied into the project.

## Completed software state

- Added a fixed-capacity 8-32-byte parser, vendor B1/B8/UID decoding, XOR-NOT
  checksum, one-pending-command reader state machine, 120 ms command gap,
  2 s health query, three-attempt timeout, and periodic handshake recovery.
- Added a 64-byte RX-to-idle DMA buffer, 256-byte ISR-to-worker ring, and one
  ThreadX worker. ISR work is bounded to byte copy plus semaphore notification;
  parsing and transmission stay in thread context.
- Added generated `config::feature::has_rfid`, `app::uart::rfid`, and
  `params::rfid`, with missing UART/DMA and remoter/referee/test-report
  collision checks.
- Added the zero-motor `f407-rfid-uid-debug` preset: RFID on USART6 at 9600
  8N1 and change-driven text reporting on USART1 at 115200.
- Passively starts RFID in the PS2 combined image and exposes its state only
  through the existing debug snapshot; RFID never enters the chassis/ARM
  control or terminal-fault path.

## Verification

- Fresh full Host rebuild and CTest: 61/61 passed.
- `f407-rfid-uid-debug`: linked; 42,848 B Flash and 52,976 B RAM.
- `f407-mycar-combined-ps2-debug`: linked; 110,408 B Flash and 64,032 B RAM.
- Generated debug config: RFID USART6, test-report USART1, address 32, baud
  9600, `PNX_F407_CAN_ENABLED=0`, `PNX_F407_USART_ENABLED=1`.
- Generated combined config: RFID USART6, no test-report UART.
- `git diff --check` passed. Submodule commits remain unchanged and clean.

## Remaining hardware checks

- Confirm the physical reader is the TTL variant and measure UART levels
  before direct STM32 connection; an RS232 variant requires a transceiver.
- Verify UID against the vendor tool, B1/B8 responses, static and moving read
  reliability, detection latency, adjacent-tag behavior, unplug/replug
  recovery, and failure isolation while PS2 vehicle control is active.
- No hardware communication claim has been made. The implementation was
  committed as `27ee62a6b1190d328215d62b400c0fc2732385cd` and pushed to
  `origin/feat/rfid`. Build products under `build/`, including the ELF, remain
  intentionally local and ignored by Git.
