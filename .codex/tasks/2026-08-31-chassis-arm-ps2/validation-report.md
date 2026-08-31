# Validation report

## Baseline — 2026-08-31

- `cmake -S . -B build/host-ps2-baseline -G Ninja -DPNX_HOST_TESTS=ON`: PASS.
- `cmake --build build/host-ps2-baseline --parallel 4`: PASS.
- `ctest --test-dir build/host-ps2-baseline --output-on-failure`: 57/57 PASS.
- `cmake --preset f407-mycar-combined-debug --fresh`: PASS.
- `cmake --build --preset f407-mycar-combined-debug --parallel 4`: PASS;
  RAM 60,920 B, Flash 98,640 B.
- Hardware/flash: NOT RUN.

## Implementation validation

- Focused Host tests: 6/6 PASS (`combined_ps2_input_adapter`, mode router,
  chassis policy, ARM policy, product contract, runtime source contract).
- Full Host suite: 58/58 PASS.
- Fresh `f407-mycar-combined-ps2-debug`: PASS; RAM 60,976 B; Flash 101,692 B;
  SHA-256 `C83A971B34F584952BAF25A5A0668623D0AC159DB72FD975EE6F959C70F8A79B`.
- Fresh `f407-mycar-combined-debug`: PASS; RAM 60,936 B; Flash 100,112 B;
  SHA-256 `657FE24EC921E9BD71862C8E913F8CF8E280DE7B269FCEF2C708CE124FB0FA77`.
- PS2 graph: `ps2.cpp` present, `dr16.cpp` absent. DR16 graph: `dr16.cpp`
  present, `ps2.cpp` absent.
- Both graphs: no H7, DM IMU, referee, TIM6, or heating-lease match.
- Generated flags: PS2 `ENABLE_DR16=0`, `ENABLE_PS2=1`, USART1; DR16
  `ENABLE_DR16=1`, `ENABLE_PS2=0`, USART3.
- `git diff --check`: PASS with existing LF-to-CRLF notices only.
- Four submodules: clean and unchanged.
- Flash/hardware/Vault: NOT RUN / NOT MODIFIED.

## Operator-feedback revision

- The earlier PS2 ELF was flashed and verified by OpenOCD. The operator
  reported that the old chassis mapping used the left stick for translation
  and the right stick for yaw, with translation axes physically crossed.
- Revised controls: Circle latches unlock; Cross locks; held R1 selects
  chassis; held R2 selects ARM; both/neither selects Neutral. Link loss and
  invalid input relock. Circle requires a fresh release/press after startup or
  reconnect.
- Second hardware feedback showed the receiver labels physical right-stick
  vertical as `right_x` and horizontal as `right_y`. Latest mapping is
  `right_x -> vx`, `right_y -> vy`, `left_x -> yaw`; ARM axes are unchanged.
- TDD RED observed for adapter, chassis policy, mode router, and PS2
  Circle-then-R2 ARM arming. Post-revision focused Host: 6/6 PASS.
- Fresh latest PS2 build PASS: RAM 60,976 B; Flash 101,960 B; SHA-256
  `386BCF5337279CEAAD968975888757480061A5EC673DFA7FEB48C8CE7FE311C2`.
- Fresh latest DR16 build PASS: RAM 60,936 B; Flash 100,380 B; SHA-256
  `F9F7BAEAD6F481A39510FFBE2AB42553575C90D1ADD40414E825816F15D6534B`.
- `git diff --check`: PASS with line-ending notices only. Four submodules:
  clean. The prior `6F3666E0...C19C8A` PS2 ELF was programmed and exposed the
  remaining axis swap. Latest `386BCF53...11C2` was programmed, verified OK,
  and reset through OpenOCD. Direction behavior remains pending operator
  testing. Full Host remains the pre-commit gate and was not rerun after this
  correction.
- Operator confirmed the translation-axis correction but reported left/right
  yaw reversed. Latest vehicle policy negates only PS2 `left_x` yaw; DR16 and
  ARM mappings remain unchanged.
- Fresh yaw-sign PS2 build: RAM 60,976 B; Flash 101,972 B; SHA-256
  `925AED920588A05949BE81AD62169593F6662E166537AC5EEF79EAEFA26C4A53`.
- Fresh DR16 rollback build: RAM 60,936 B; Flash 100,392 B; SHA-256
  `472E89FCF9991723F6564597E56E48D3388B9377D47252551A0D6460772B78A3`.
- Latest yaw-sign ELF was programmed, verified OK, and reset through OpenOCD;
  hardware direction remains pending operator confirmation.
