# F407 Engineering Handoff

## Direct BSP publication and feature integration authority — 2026-08-04

**Status: REPOSITORY PUBLICATION PASS; FRESH SOFTWARE PASS; HARDWARE NOT RUN.**

This section is the current integration authority for agents working on
`pnx_f4_mycar`. It supersedes the 2026-08-03 pre-publication snapshot below.

### Published authorities

- BSP branch: `HKUSTGZ-ROBOMASTER-PNX/pnx_bsp:F4_version_bsp` at
  `ee59c97a852586eb7f6fb50b402f2fe9ed112cbc`
  (`feat(pwm): support four F407 servo channels`). The branch was pushed and
  re-read from GitHub before the parent was published.
- Validated parent code head:
  `shiuhou/pnx_newtemplate_F4:feat/chassis@c4d8ebcde95d18f7873fbca66b7635ee54b1f1df`.
  This handoff is published as a docs-only successor; use `git rev-parse HEAD`
  for the containing branch's final documentation commit. It changes no code
  or submodule gitlink.
- Code/architecture commits, oldest first:
  1. `54ea8c22b3d8ff79ea72af51db98d56f27a10fa9` — select the direct F407 BSP
     implementations and update the BSP gitlink;
  2. `14649d26a693fdf2d2a5c2ed7121e81957f1b421` — require explicit board
     configuration capabilities;
  3. `82ae3c5b8fa1682650a054aa6f0c9ea05c0696f7` — build all six supported F407
     presets in CI;
  4. `1c5a739b2b819b157ba0534e30167b5512b29e6c` — move 16 unused demo files to
     `demo/_reference` with `R100` identity;
  5. `c4d8ebcde95d18f7873fbca66b7635ee54b1f1df` — document Direct BSP ownership
     and navigation boundaries.
- The official reference branch was checked immediately before publication
  and remained `pnx_template/F4_version@9f8707280d56ae1c8f8061cb9ec12e75f527d1f8`.
- Shared gitlinks remain unchanged and clean in the published tree:
  - `pnx_devices@2349cc108c9ed477ccdcd700e802ea888975cdfd`;
  - `pnx_libs@e7c3e7a2b9d825586ab3e0c413877180c4295df8`;
  - `pnx_modules@8ba925b60b11fec511a57622c199b57bb23f8f4e`.
- No tag was created. No Vault write or hardware operation was performed.

### Architecture boundary

The published flow is:

```text
vehicle / device / module
        -> public HAL-free bsp::* API
        -> selected pnx_bsp/<peripheral>/src F407 implementation
        -> STM32 HAL / generated Board resources
```

The 15 former `boards/dji_c_board_f407/bsp/*` implementation/helper files are
removed from the parent. No `detail::backend_*`, runtime backend registry,
factory or application framework was introduced. The public BSP API semantics
remain unchanged. The accepted four-channel TIM1 PWM mapping is PE9/CH1,
PE11/CH2, PE13/CH3 and PE14/CH4; shortening the shared period rejects any
value below an existing channel pulse/CCR.

### Fresh recursive-clone evidence

A new recursive clone of the remote `feat/chassis` branch resolved the exact
parent and four submodule SHAs above. The tracked parent and every submodule
were clean after validation.

- Native Host configure/build/CTest: **44/44 PASS**.
- All six remote-reproducible images configured, clean-built and linked with
  zero compiler/linker warning lines and exactly one ELF:

| Preset | Configure | Build/link | Warnings |
| --- | --- | --- | ---: |
| `f407-debug` | PASS | PASS | 0 |
| `f407-release` | PASS | PASS | 0 |
| `f407-usb-cdc-debug` | PASS | PASS | 0 |
| `f407-usb-cdc-release` | PASS | PASS | 0 |
| `f407-pwm-a2-debug` | PASS | PASS | 0 |
| `f407-mycar-chassis-debug` | PASS | PASS | 0 |

- Core/USB/PWM/MyCar source-graph isolation: PASS.
- CAN/USART generated-init authority: false in the five non-MyCar images and
  true only in the MyCar image.
- Every ELF has exactly one `app_start` and no duplicate strong public
  `bsp::*` symbol.
- Public BSP header HAL/handle leakage: none in the checked token set.
- Production `detail::backend_*`/`pnx_backends` forwarding: none.
- Retired CAN/M2006, BMI088 and standalone DBUS selectors: explicit
  fail-fast PASS.
- A repository-wide scan found five historical absolute Windows paths in
  already tracked documentation. The set is identical to the `ff1897e`
  baseline; this publication added zero absolute-path references and no
  absolute-path-only artifact.
- MaixCam files and their Python tests are not in the published commit series;
  the earlier live-worktree 29/29 result is not fresh-clone evidence for this
  branch.
- Servo, motor, CAN, IMU, DBUS, USB and all other physical hardware
  revalidation: **NOT RUN**.

### Live worktree preservation and agent rule

The local worktree at
`C:\Users\USER\Desktop\RM\rm_inschool\2026\firmware\pnx_f4_mycar` intentionally
remains on `feat/chassis@ff1897e3bc890bf6078aca08bc2d85064f0d40ca`
with its pre-existing staged, unstaged and untracked chassis, MaixCam,
ARM/PWM, DR16 and documentation work. It was not reset, cleaned, rebased,
merged or fast-forwarded during publication. Its `pnx_bsp` checkout is clean
at `ee59c97`, while the local parent HEAD still records the older BSP gitlink;
the clean remote branch above is the reproducible publication authority.
The live `pnx_modules` edits remain separate feature work and were not changed.

Feature agents must not repeat the migration by copying old Board BSP files.
They should inspect their own dirty state, fetch `origin/feat/chassis`, then
integrate the five parent commits in order (or merge the published branch when
safe). Update only the `pnx_bsp` submodule to the recorded gitlink; do not move
`pnx_devices`, `pnx_libs` or `pnx_modules`. Resolve CMake/test overlaps without
discarding feature changes, rerun the branch-specific Host tests and affected
preset, and record hardware as `NOT RUN` unless it was actually observed.

```text
PNX_BSP_COMMIT_PUSH=PASS
PARENT_COMMIT_PUSH=PASS
FRESH_RECURSIVE_CLONE=PASS
HOST_CTEST=44/44_PASS
F407_SIX_PRESETS=PASS
SOURCE_GRAPH_AND_SYMBOL_GATES=PASS
HARDWARE_REVALIDATION=NOT_RUN
VAULT_WRITE=NOT_RUN
```

## Superseded pre-publication snapshot — 2026-08-03

**Status at that date: LOCAL SOFTWARE PASS; publication and hardware
validation were NOT RUN. This section is superseded by the 2026-08-04
authority above.**

This section is retained as dated pre-publication evidence. It is no longer
the current authority for branch, remote SHA, BSP publication state or
fresh-clone results.

### Repository state

- Repository:
  `C:\Users\USER\Desktop\RM\rm_inschool\2026\firmware\pnx_f4_mycar`.
- Parent branch/HEAD: `feat/chassis` at
  `ff1897e3bc890bf6078aca08bc2d85064f0d40ca`.
- The parent is an intentionally mixed working tree containing pre-existing
  chassis, MaixCam, ARM/PWM and DR16 work. This integration was applied in
  place so those changes remain available together; it is not a clean release
  checkout.
- Direct-BSP parent reference:
  `pnx_template/F4_version@9f8707280d56ae1c8f8061cb9ec12e75f527d1f8`.
  Its accepted source-layout changes were ported selectively; the branch was
  not merged or rebased over the mixed worktree.
- `pnx_bsp` checkout: detached
  `da09febe8f5bfe66993010247d4d6731d0ca492b`
  (`origin/F4_version_bsp`) plus two local four-channel PWM modifications:
  `pwm/src/bsp_pwm.cpp` and `pwm/src/pwm_channels.hpp`.
- The parent index still records the older `pnx_bsp` gitlink
  `c097c5b581baf9b8cb7cff90bc8a22d2136a2437`. This is deliberate while no
  BSP or parent commit/push is authorized; the integrated state is therefore
  **not yet reproducible from the parent commit alone**.
- Shared pins remain:
  - clean `pnx_devices@2349cc108c9ed477ccdcd700e802ea888975cdfd`;
  - clean `pnx_libs@e7c3e7a2b9d825586ab3e0c413877180c4295df8`;
  - `pnx_modules@8ba925b60b11fec511a57622c199b57bb23f8f4e`
    with the three pre-existing DR16/remoter edits under
    `remoter/include/{remoter.hpp,types.hpp}` and `remoter/src/dr16.cpp`.
- Three pre-existing MaixCam files (`detector.py`, `main.py`, `vf_config.py`)
  changed again after the integration preservation snapshot. They were not
  modified, restored or interpreted by this task and remain owned by that
  parallel feature effort.
- No commit, push, tag, merge, rebase, CubeMX/IOC regeneration, hardware
  operation or Vault write was performed.

### Task objective

Bring the current MyCar feature work onto the accepted F407 Direct BSP source
layout once, without asking every feature agent to repeat the migration and
without losing or normalizing their in-progress changes. Future agents should
read this section first and continue from the live checkout only after
rechecking `git status` and submodule state.

### Actual changes

- Board-selected implementations now compile from
  `pnx_bsp/<peripheral>/src`; the 15 former
  `boards/dji_c_board_f407/bsp/*` implementation/helper files are removed from
  the parent source tree. Public callers still use the same HAL-free `bsp::*`
  API; no `detail::backend_*`, runtime registry or factory was introduced.
- The four-channel TIM1 PWM implementation from the existing ARM work was
  transplanted without loss into `pnx_bsp/pwm/src` before the parent copies
  were removed, then received the review-driven period-shrink fail-closed
  guard described below. Its mapping remains PE11/CH2, PE9/CH1, PE13/CH3 and
  PE14/CH4 with one shared timer period and independent channel pulse/start
  state.
- Root and Host CMake now select the direct BSP sources and private BSP helper
  include directories. The source-layout contract checks that direct sources
  live only in `pnx_bsp` and that no forwarding symbol returns.
- Image capability remains build-time selected: Core contains no optional
  BSP; USB adds only USB; PWM adds only PWM; MyCar adds CAN, USART and its
  vehicle closure. The existing compile-time CAN/USART lifecycle authority is
  retained, so only MyCar executes those generated peripheral init calls.
- Disabled-by-default shared configuration compatibility was aligned with the
  F407 template while preserving MyCar motor/remoter generation.
- CI now lists all six supported F407 presets, including the MyCar image.
- Historical demo references moved under `demo/_reference`; `demo/README.md`
  distinguishes the product `demo/app.cpp` entry point from references.
- Documentation and source-contract paths were updated to the new BSP source
  ownership. `pnx_devices` and `pnx_libs` were not changed; the dirty
  `pnx_modules` remoter work was not edited by this integration.

### Fresh software verification

The final verification commands and complete observations are also recorded
in `.codex/tasks/2026-08-03-mycar-direct-bsp-sync/validation-report.md`.

- Native Host configure/build and CTest: **45/45 PASS**.
- MaixCam Python Host suite: **29/29 PASS**.
- All six F407 presets configured and clean-built with zero compiler/linker
  warning lines and a linked ELF:

| Preset | Configure/build/link | Warnings | RAM | Flash |
| --- | --- | ---: | ---: | ---: |
| `f407-debug` | PASS | 0 | 49,792 B | 23,736 B |
| `f407-release` | PASS | 0 | 49,792 B | 15,188 B |
| `f407-usb-cdc-debug` | PASS | 0 | 66,040 B | 67,952 B |
| `f407-usb-cdc-release` | PASS | 0 | 66,000 B | 40,056 B |
| `f407-pwm-a2-debug` | PASS | 0 | 49,840 B | 28,240 B |
| `f407-mycar-chassis-debug` | PASS | 0 | 57,992 B | 72,972 B |

- Compile-database graph checks found: Core has no CAN/USART/USB/PWM direct
  BSP or consumer closure (generated dormant `can.c`/`usart.c` remain); USB
  and PWM add only their own direct BSP source; MyCar adds one CAN source, one
  USART source and its nine vehicle translation units.
- Generated init authority is false for CAN/USART in the five non-MyCar
  images and true only in the MyCar image.
- Every ELF contains exactly one `app_start`; no ELF has duplicate strong
  public `bsp::*` definitions.
- Public `pnx_bsp` headers contain none of the checked STM32 HAL headers,
  handle types or global handle names. A production-source scan found no
  `detail::backend_*` forwarding symbol.
- `git diff --check`, cached-diff check and the `pnx_bsp` diff check pass.
- All 16 relocated demo reference files have the same Git blob identity as
  their pre-move `HEAD` paths.
- Of 40 pre-existing dirty-file preservation hashes, 36 remain identical.
  The four expected/observed differences are the intentionally updated
  `vehicle/arm/HANDOFF.md` plus three separately evolving MaixCam files named
  in the repository-state section; none was reverted.

### Failed attempt and correction

An initial ELF-symbol audit searched every demangled line containing
`bsp::`. It falsely reported the two ABI constructor aliases of
`vehicle::chassis::runtime_policy(..., bsp::can::telemetry)` at the same
address as duplicate BSP definitions. The checker was corrected to inspect
only symbol names beginning with `bsp::` and to count distinct strong-symbol
addresses. The complete six-image audit then passed. Firmware source and link
output were not changed to satisfy this checker.

Two other audit-script assumptions were corrected without changing firmware:
the MyCar compile database authoritatively contains nine, not ten,
`vehicle/chassis` translation units; and a raw backend-string scan matched the
Host contract's deliberate forbidden-token sentinel. The final graph uses the
nine CMake-declared files and the backend scan is limited to production
sources, while the Host contract continues to test the forbidden token.

The final independent review then found that shortening the shared PWM period
could leave an existing channel compare above the new ARR. A test-first fix
now rejects that change with `invalid_arg` and preserves both the old period
and pulse state. The Host behavior test and F407 source contract were each
observed failing before the implementation change, then passing afterward.
The guard adds 116 bytes of Flash to the PWM validation image and does not
change its RAM occupancy.
The follow-up independent read-only review marked that finding resolved and
reported no new Critical or Important issue. The remaining publication
blocker is the intentionally uncommitted BSP/gitlink state, not a local build
or Direct BSP architecture failure.

### Decisions and evidence boundary

- The Direct BSP migration is a source-ownership change, not a public API or
  application architecture redesign.
- The BSP checkout intentionally remains dirty because the accepted upstream
  commit has one-channel PWM while the MyCar ARM work needs the already tested
  four-channel implementation. Publishing it requires an explicit BSP commit
  before the parent gitlink can be updated.
- Existing staged and unstaged feature work was preserved in place. A
  pre-change hash inventory was used as a guard; later parallel MaixCam edits
  are explicitly attributed as outside this integration rather than reverted.
- Build, link, Host tests and static source/symbol checks are software
  evidence only. No servo, motor, CAN bus, DR16 receiver, USB device or other
  physical hardware was exercised in this task.

```text
DIRECT_BSP_LOCAL_INTEGRATION=PASS
HOST_CTEST=45/45_PASS
MAIXCAM_HOST=29/29_PASS
F407_SIX_PRESETS=PASS
SOURCE_GRAPH_AND_SYMBOL_GATES=PASS
PNX_BSP_COMMIT_PUSH=NOT_RUN
PARENT_COMMIT_PUSH=NOT_RUN
FRESH_RECURSIVE_CLONE=NOT_RUN
HARDWARE_REVALIDATION=NOT_RUN
VAULT_WRITE=NOT_RUN
```

### Risks, next actions and rollback

1. Every feature agent should read this section, then inspect the live status;
   it should not independently re-copy the old Board BSP files or reset the
   submodule.
2. Review the two PWM diffs in `pnx_bsp`, commit and push them on the intended
   `F4_version_bsp` publication branch, then deliberately stage the parent
   deletions/CMake/tests/docs and updated `pnx_bsp` gitlink. Do not use broad
   `git add -A` in this mixed worktree.
3. Preserve the current `pnx_devices`/`pnx_libs` pins and resolve the existing
   `pnx_modules` DR16 work on its own feature provenance; do not manufacture a
   shared-module branch merely for this layout migration.
4. After publication, prove reproducibility with a fresh recursive clone and
   rerun the six presets, 45 Host tests, MaixCam suite and source/symbol gates.
5. Hardware validation remains a separately authorized, attended activity.

Rollback anchors are parent
`ff1897e3bc890bf6078aca08bc2d85064f0d40ca`, pre-task BSP checkout
`c097c5b581baf9b8cb7cff90bc8a22d2136a2437`, and target BSP base
`da09febe8f5bfe66993010247d4d6731d0ca492b`. Because unrelated changes are
mixed into the worktree, rollback must reverse only the paths/hunks listed in
the task record. Never use `git reset --hard`, `git clean`, or a broad checkout
restore.

## MyCar DR16 + four-M2006 chassis software gate — 2026-07-31

**Status: SOFTWARE PASS; hardware gated.** The final cross-task review found
startup switch-history, fault-reset and runtime-fault PI-reset gaps. Those
findings were corrected test-first, passed fresh validation and were
independently re-reviewed with no remaining Critical or Important code issue.

This section is the current authority for the uncommitted `mycar/f4`
vehicle worktree. It supersedes the older MyCar planning-only statements in
this file, but it does not supersede the public F407 architecture history
below.

### Scope and provenance

- Repository: `pnx_f4_mycar`; branch: `mycar/f4`.
- Baseline HEAD remains
  `ed9a2271371c61001fd7440f413b542c2ba64218`; all vehicle work is an
  unstaged working-tree change.
- Pinned, clean submodules:
  - `pnx_bsp@c097c5b581baf9b8cb7cff90bc8a22d2136a2437`
  - `pnx_devices@2349cc108c9ed477ccdcd700e802ea888975cdfd`
  - `pnx_libs@e7c3e7a2b9d825586ab3e0c413877180c4295df8`
  - `pnx_modules@8ba925b60b11fec511a57622c199b57bb23f8f4e`
- The public `pnx_f4_minimal` worktree remains clean on `F4_version` at
  `ed9a2271371c61001fd7440f413b542c2ba64218`.
- No source was staged, committed or pushed. No remote, shared submodule,
  Board/CubeMX file, Vault file or hardware state was changed.
- Review snapshot files under `.superpowers/sdd/` were removed on 2026-08-01;
  their contents can be reconstructed with `git log` and `git diff`.
- The pre-existing untracked `demo/chassis_mecanum/` remains preserved and is
  not part of this implementation. An early Windows exclusion error exposed
  exactly four matching source-line snippets from that directory; they were
  treated as tainted and were not used or cited for any design decision. The
  file was not opened or modified, and later work used exact allowlisted
  paths.

### Implemented vehicle-local software

- Pure `vehicle/chassis` modules implement X-mecanum inverse kinematics,
  continuous deadband/manual mapping, startup-safe arming, sticky health-loss
  handling, four explicit-dt PI loops, uniform wheel-target saturation and
  bounded signed raw-current formation.
- Coordinates are `+x` forward, `+y` left and `+yaw` counter-clockwise;
  wheel order is FL/FR/RL/RR.
- MyCar generated configuration selects DR16 on USART3 and four M2006s on
  classic CAN1 feedback IDs `0x201` through `0x204`, all initially in
  `relax` mode.
- The MyCar-only ThreadX adapter owns the four static M2006 objects, one-shot
  registration, a 200 Hz control thread and a separate remote-ingest thread.
  The control thread never calls the potentially unbounded message read.
- Remote input is forced offline when unseen or older than 120 ticks. Motor
  health is sampled every four control iterations. Retained CAN
  error/drop/fault-epoch changes and timing overruns latch zero output.
- Offline/invalid remote samples cannot satisfy the startup release interlock.
  Controller health faults clear only after a fresh online switch release;
  runtime-sticky faults inhibit the controller and every overrun path resets
  PI state. Non-finite manual samples fault-latch an armed controller and
  always select zero output.
- Telemetry publication and reads are coherent whole-object copies. The
  runtime applies neither direction nor current clamping twice.
- The default product configuration deliberately retains zero sentinels for
  unmeasured geometry, speed limits, PI limits and current limit. It is
  invalid by construction, latches `invalid_config`, and the built MyCar
  image is therefore a zero-only integration image.
- Independent task reviews approved Tasks 1-6 after correction. Final review
  status has Critical 0 and Important 0. The only retained nonblocking note
  is that the source-order contract is intentionally a textual regression
  check rather than an AST check; the Task 5 follow-up has no remaining
  finding.

### Fresh final software evidence

- Host configure/build: PASS, `49/49` Ninja steps, zero compiler/linker
  warnings.
- Host CTest: **41/41 PASS**, including kinematics, manual/safety,
  controller, fail-closed configuration, runtime policy and runtime source
  ordering contracts.
- All six visible F407 presets were configured and rebuilt with
  `--clean-first`; every build linked with zero compiler/linker warnings and
  exactly one `app_start`:

| Preset | Steps | RAM | Flash | ELF SHA-256 |
| --- | ---: | ---: | ---: | --- |
| `f407-debug` | 199/199 | 49,792 B | 23,736 B | `76533AB440EA66ECA26EA7F20781BD74D547B929038F0200F46596EB79E9A9AD` |
| `f407-release` | 199/199 | 49,792 B | 15,188 B | `AC9B2ECA96CEDE40C8F9B3705193347CBCE758EC4D22D4E412F6166488B89D91` |
| `f407-usb-cdc-debug` | 311/311 | 66,040 B | 67,952 B | `9BEE15E7687E97DA9BC62C45CAC63B452A551B6DEF9ACEDABA6E4613F5499847` |
| `f407-usb-cdc-release` | 311/311 | 66,000 B | 40,056 B | `95A1984239B22410D5D8B3936E01506F8E31B61877B2DC53035B8550ADA529A3` |
| `f407-pwm-a2-debug` | 200/200 | 49,864 B | 27,656 B | `B851F9DCDA2AA571892571DFEB1644BB58AEBB9A53084B744574BC5C4B191D99` |
| `f407-mycar-chassis-debug` | 215/215 | 55,368 B | 69,448 B | `D6A2EDD14562D15D0065B4FD71E3F17E3E502C77C70869DB57E2F3892EB5003E` |

- Compile-database checks found zero vehicle, CAN, USART, DR16, remoter or
  DJI-motor closure in the five non-MyCar presets. MyCar contains exactly one
  copy of each of the nine vehicle translation units, direct CAN/USART BSP
  sources, DR16/remoter sources and three DJI motor sources.
- Generated-config checks found all five non-MyCar presets disabled for
  remoter/DR16/motors. MyCar has DR16/USART3, four DJI motors and exactly four
  generated `mode::relax` initializers at IDs `0x201..0x204`.
- ELF checks found no vehicle/M2006/DR16 symbol leakage in the five normal
  images. The MyCar ELF retains runtime start, DR16, all four wheel motor
  objects, both ThreadX thread/stack pairs, controller and runtime policy.
- The pure control-module allowlist from `types` through `controller` and
  configuration has no HAL, ThreadX, BSP, message, remoter or motor
  dependency. `git diff --check` passes; the Git index, four submodules and
  public worktree are clean.

GNU `nm` initially rejected Unicode absolute ELF paths on this Windows host.
The symbol checks were rerun successfully from each build directory with an
ASCII relative ELF path; this was a tool-path limitation, not a firmware
build failure.

### Evidence boundary and next gates

```text
TASKS_1_TO_6_SOFTWARE=PASS
TASK_7_PHYSICAL_MEASUREMENTS=NOT_RUN
TASK_8_FLASH_AND_HARDWARE=NOT_RUN
DEFAULT_MYCAR_IMAGE=INVALID_CONFIG_ZERO_ONLY
SAME_CYCLE_CAN_SEND_ACCEPTANCE=UNKNOWN
WCET_AND_STACK_HIGH_WATER=NOT_RUN
COMMIT_STAGE_PUSH=NOT_RUN
VAULT_WRITE=NOT_RUN
```

Wheel radius, wheelbase, track, wheel-to-ID identity and physical positive
directions still require power-off/attended measurement. Flashing, live DR16
and CAN feedback, zero-current bench checks, non-zero current, PI tuning,
lifted-wheel tests, ground motion, timing measurements, three repetitions and
the 120-second soak were not performed. The pinned
`djimotorhandler::send_control()` returns `void`, so same-cycle CAN acceptance
cannot be claimed from this software task; only command formation and delayed
CAN telemetry are observable.

## Latest pnx_template main alignment — local working tree, 2026-07-30

This section supersedes the shared-pin and current-working-tree statements
below. It records a local-only alignment with
`pnx_template/main@cf6577765358822a1bc57c1ea17fe65a795ceb62`.
No commit, push, tag, remote change, CubeMX regeneration, or hardware
operation was performed.

### Compared remote state

- Official `pnx_template/main` is
  `cf6577765358822a1bc57c1ea17fe65a795ceb62`.
- Official `pnx_template/F4_version` remains
  `aafa57c7f0b7f20276efa14fd74cbc060cc2902b`. It is a merge of the prior
  F407 work and `cf65777`, but deliberately retained the older shared
  Device/Lib/Module gitlinks.
- The Feishu workflow blob is already identical on official `main` and
  `F4_version`. No local workflow change is required.
- H7 USART10 Board/IOC changes, H7 demo composition, temporary IMU
  live-watch diagnostics, `.clangd`, and `.settings` files were not ported
  into the F407 architecture.

### Local modifications for review

- Parent base:
  `refactor/f407-minimal-architecture@d9d85acdca5a82a330762e02c7dd9b0f580d54bb`.
- Updated exact shared pins:
  - `pnx_devices@2349cc108c9ed477ccdcd700e802ea888975cdfd`
  - `pnx_libs@e7c3e7a2b9d825586ab3e0c413877180c4295df8`
  - `pnx_modules@8ba925b60b11fec511a57622c199b57bb23f8f4e`
- `pnx_bsp` remains on the Direct BSP lineage at
  `4d3ce2abb3dee18ad551cb03428563b38e384050` with an uncommitted,
  board-neutral USART line-configuration declaration. The F407 Board source
  directly implements it; no `detail::backend_*` or runtime registry was
  added.
- The F407 config generator now emits disabled-by-default PS2 feature,
  binding, and timing symbols plus LK8016/LK9025 model identifiers. It does
  not add PS2, AHRS, BMI088, or motor source to an RC2 image.
- Host contracts cover the new USART API, generated PS2/LK config, and the
  latest BMI088/LK9025/PID/PS2 public API surface.

### Fresh local evidence

- Host configure/build and CTest: **35/35 PASS**.
- All five F407 presets configure, compile, and link with zero warnings.
  RAM/Flash usage is unchanged from the prior RC2 table.
- Every ELF contains exactly one `app_start` and no duplicate strong
  `bsp::*` definition.
- Core, USB, and PWM command graphs remain isolated. No graph includes CAN,
  USART, SPI, Flash, Device, Module, PS2, BMI088, or retired validation
  source; USB and PWM add only their respective closures.
- All three retired selectors still fail fast.
- GNU Arm `-Wall -Wextra -Werror -fsyntax-only` passes for the modified
  F407 `bsp_usart.cpp` and the latest PS2, merged Remoter, DR16, VT03, and
  Referee consumers.
- Public shared headers contain no STM32 HAL type or handle leakage.

### Deferred findings retained

- The latest BMI088 source still requires a Board-owned
  `GYRO_INT_Pin`/EXTI mapping. A focused F407 syntax check stops at that
  unresolved symbol, so BMI088/AHRS remains
  `NOT_IN_RC2_PRODUCT_GRAPH`.
- The latest AHRS service directly contains the Tactical EKF and an enlarged
  8192-byte IMU thread stack. Because AHRS is outside RC2, no runtime,
  memory-budget, or hardware claim is made for it.
- Shared Remoter sources still use the legacy `RAM_D1_BSS` portability macro;
  current `memory.h` maps it for non-H7 builds, but a future shared cleanup
  may rename it without creating an F407 fork.

```text
PNX_TEMPLATE_BASELINE=cf6577765358822a1bc57c1ea17fe65a795ceb62
PNX_DEVICES_TEMPLATE_REUSE=PASS
PNX_LIBS_TEMPLATE_REUSE=PASS
PNX_MODULES_TEMPLATE_REUSE=PASS
PNX_BSP_DIRECT_CONTRACT=LOCAL_UNCOMMITTED
HOST_TESTS=35/35_PASS
F407_5_PRESETS=PASS
SOURCE_GRAPH_ISOLATION=PASS
HARDWARE_REVALIDATION=NOT_RUN
PARENT_PUSH=NOT_RUN
RC2_TAG=NOT_CREATED
```

## RC2 release-closure result — 2026-07-30

This section is the current authority for
`F407-DIRECT-BSP-RC2-RELEASE-CLOSURE`. The user corrected two acceptance
definitions after the first closure report: H7 is out of scope, and generated
Board source presence alone is not Direct-BSP closure leakage. Parent
publication and tagging remain pending, but the user-authorized compile-time
guards now satisfy the corrected F407 Core call-graph Gate. No hardware was
operated in this closure.

### Git and architecture state

- Initial parent:
  `refactor/f407-minimal-architecture@a9d63a3c7c733529fe02df181b8dfcaeb09d0e00`.
- The Direct BSP architecture/build/test change is committed locally as
  `2e4b80d211bb660d08bf48f3484d2922a80fff85`. Parent push,
  official-branch integration, fresh remote clone, and the RC2 tag are still
  pending at this snapshot.
- `pnx_bsp` has one local atomic candidate commit:
  `4d3ce2abb3dee18ad551cb03428563b38e384050`
  (`refactor: finalize board-selected direct BSP contracts`). Its worktree is
  clean. It is remotely reachable through official
  `pnx_bsp/F4_version@c83e892d9ba76e2671e8d1c8fbc2939a7a77e9df`;
  that non-force merge tip differs from the tested candidate only by retaining
  the official Feishu workflow. The previous H7-based stop condition is void.
- Template-shared pins remain source-clean and exact:
  - `pnx_devices@8a6783e63d77a15940aea8245bbe2eb13a2f2b11`
  - `pnx_libs@55bd94060b7be562ce7a6773822a6a4d2bcab9c0`
  - `pnx_modules@a54c493020ba9bcd5b43b99e068a06cdda9dd018`
- Public BSP headers have zero forbidden STM32 HAL/MCU-handle matches.
- F407 sources and host fakes directly define public `bsp::*`; no
  `detail::backend_*` or `pnx_backends` implementation is restored.
- The official targets exist at
  `pnx_template/F4_version@cf6577765358822a1bc57c1ea17fe65a795ceb62`
  and `pnx_bsp/F4_version@c83e892d9ba76e2671e8d1c8fbc2939a7a77e9df`.
  The local parent has no common ancestor with the official parent branch;
  the BSP branches share a base but have diverged. A force push is prohibited,
  so official integration remains a separate reviewed non-force step.

### USB contract closure

- `init(ok)` means raw config/callback ownership accepted, required ThreadX
  resources created, and asynchronous startup worker scheduled. It does not
  mean enumerated, host-connected, or CDC-ready.
- `connected()` is true only while the controller and CDC transport are
  usable. Controller/activation/resource failure enters observable
  `link_state::fault`.
- `write()` is bounded and copies caller data. Before ready, after disconnect,
  or when the bounded queue cannot accept data it returns a non-`ok` status.
- Same-config re-init is idempotent. Incompatible callback, user context, or
  config is rejected without replacing ownership.
- `fill_tx` runs in the BSP worker, `on_rx` in the USBX bulk-OUT thread, and
  `on_tx_done` in the USBX bulk-IN thread. Buffers live only for the callback;
  callbacks must not block. Disconnect retires queued/in-flight ownership and
  suppresses late success.
- Fresh native configure/build and CTest:
  **33/33 PASS**, including resource failure, async success, controller
  failure, ready transition, write-before-ready, pending-TX disconnect,
  re-init/ownership, identity fail-closed, and 64-byte/ZLP regressions.

The CubeMX PCD initialization still runs before the BSP lifecycle exists.
Its failure follows the existing diagnostic fail-stop `Error_Handler()` path;
it is observable fail-stop, not a BSP runtime-state transition.

### Final local F407 software evidence

All builds used fresh ASCII temporary build directories and GNU Arm
13.3.1. Configure, compile, and link passed with zero compiler/linker warnings:

| Preset | RAM | Flash | `app_start` | duplicate strong `bsp::*` |
| --- | ---: | ---: | ---: | ---: |
| `f407-debug` | 49,792 B | 23,736 B | 1 | 0 |
| `f407-release` | 49,792 B | 15,188 B | 1 | 0 |
| `f407-usb-cdc-debug` | 66,040 B | 67,952 B | 1 | 0 |
| `f407-usb-cdc-release` | 66,000 B | 40,056 B | 1 | 0 |
| `f407-pwm-a2-debug` | 49,864 B | 27,656 B | 1 | 0 |

The three retired selectors for CAN/M2006, BMI088, and DBUS RX each failed
configure with exit code 1 and the intended retirement message.

Direct-BSP source selection is isolated: Core links no
`bsp_can.cpp`/`bsp_usart.cpp`/`bsp_spi.cpp`/`bsp_flash.cpp`, USB adds only its
USB closure, and PWM adds only its PWM closure. Generated Board source
presence is not leakage by itself. CMake now derives CAN/USART capability
macros from membership of the corresponding Direct BSP source in
`PNX_EMBEDDED_SOURCES`; there is no second cache option or runtime selector.
`main.c` guards the CAN1/CAN2 and USART1/3/6 headers and init calls with those
derived values. All five RC2 images compile both values as `0`; fresh ELFs
contain zero matching calls, zero retained `MX_*_Init` symbols, and zero
`HAL_CAN_Init`/`HAL_UART_Init` symbols. Generated `can.c` and `usart.c` still
compile as dormant Board capability source. Direct `bsp::can::init()` and
`bsp::usart::init()` do not call `MX_*_Init` or HAL peripheral init again.

H7 is a separate BSP implementation and release path. No H7 result is an RC2
finding or Gate:

```text
H7_REGRESSION=OUT_OF_SCOPE
```

### Deferred capability and release boundary

```text
FLASH_CAPABILITY=UNSUPPORTED_UNTIL_RESERVED_PARTITION
BMI088_AHRS_CAPABILITY=NOT_IN_RC2_PRODUCT_GRAPH
USB_TYPED_ADAPTER=UNSUPPORTED_UNTIL_CONTRACT_FIX
HARDWARE_REVALIDATION=NOT_RUN
H7_REGRESSION=OUT_OF_SCOPE
SOURCE_GRAPH_ISOLATION=PASS
PNX_BSP_PUSH=PASS
PARENT_PUSH=NOT_RUN
FRESH_RECURSIVE_CLONE=NOT_RUN
RC2_TAG=NOT_CREATED
```

Historical hardware observations below remain evidence for their exact older
artifacts and scopes. They are not RC2 hardware revalidation.

## Pre-RC2 direct BSP working-tree record — 2026-07-30

This older section is retained as historical engineering evidence. Its
build counts and current-support statements are superseded by the RC2
release-closure result above.

### Scope and Git state

- Repository: `pnx_f4_minimal`
- Parent branch: `refactor/f407-minimal-architecture`
- Parent baseline: `a9d63a3c7c733529fe02df181b8dfcaeb09d0e00`
- Pre-existing dirty path at task intake: `AGENTS.md`
- No commit, push, tag, remote change or Vault write was performed by this
  task. User-authorized Core, servo, CAN/M2006, BMI088 and DBUS hardware
  checks were performed; their exact boundaries are recorded below.
- `pnx_bsp` remains the only shared submodule with F407 architecture changes;
  its current checkout starts at
  `d827ca06cde685cccae5274a3e5c689fb7d9a6ba` and has uncommitted changes.
- The other three submodules now exactly match `pnx_template`:
  - `pnx_devices@8a6783e63d77a15940aea8245bbe2eb13a2f2b11`
  - `pnx_libs@55bd94060b7be562ce7a6773822a6a4d2bcab9c0`
  - `pnx_modules@a54c493020ba9bcd5b43b99e068a06cdda9dd018`

### Architecture result

- Public MCU-neutral contracts remain under `pnx_bsp/*/include`.
- F407 sources under `boards/dji_c_board_f407/bsp/` directly define public
  `bsp::*` symbols for CAN, diagnostics, Flash, indicator, PWM, SPI, USART and
  USB.
- The former `public -> detail::backend_* -> board backend -> HAL` forwarding
  path and `boards/dji_c_board_f407/pnx_backends` sources were removed.
- Host fakes now directly define the same public symbols. They verify API
  contracts, not F407 HAL runtime.
- Device and Module code may depend on public BSP headers; HAL handles, pins,
  DMA and IRQ details remain board-private.
- No H7 source or validation was added or claimed.

### Reproducibility cleanup

BMI088, DBUS RX and CAN/M2006 validation presets/options, demos, CI builds and
dependent host tests were removed when Device/Lib/Module returned to template
gitlinks. CMake rejects their retired selectors rather than silently producing
an incomplete image.

CAN, SPI, USART and Flash direct BSP sources remain available for future
application repositories, but no active application closure in this
repository selects them. Historical BMI088, DBUS and CAN hardware observations
later in this file remain historical only.

### Fresh local software evidence

Observed on 2026-07-30:

- Native host configure/build and CTest:
  `cmake -S . -B build/final-20260730-host -G Ninja -DPNX_HOST_TESTS=ON`,
  `cmake --build build/final-20260730-host`,
  `ctest --test-dir build/final-20260730-host --output-on-failure`:
  **23/23 PASS**.
  The suite includes direct tests of the board-private USB session/generation
  policy, USART RX publish-before-start rollback, the actual template
  `motor.hpp` default CAN configuration, and SPI/PWM compatibility entry
  points.
- Embedded presets configured and built successfully:
  - `f407-debug`: RAM 49,792 B; Flash 28,308 B.
  - `f407-release`: RAM 49,792 B; Flash 17,468 B.
  - `f407-usb-cdc-debug`: RAM 66,040 B; Flash 70,468 B.
  - `f407-usb-cdc-release`: RAM 66,000 B; Flash 41,908 B.
  - `f407-pwm-a2-debug`: RAM 49,864 B; Flash 32,188 B.
- Focused F407 ARM `-fsyntax-only` checks passed for the dormant direct
  `bsp_can.cpp`, `bsp_spi.cpp`, `bsp_flash.cpp` and `bsp_usart.cpp`.
- Focused ARM syntax checks also passed for the actual template consumers
  `pnx_devices/motors/motor/src/motorhandler.cpp` and
  `pnx_modules/remoter/src/dr16.cpp`.
- Actual Ninja command graphs:
  - Core selected only diagnostics, indicator and fault-handler direct BSP
    sources.
  - USB additionally selected `bsp_usb.cpp`.
  - PWM additionally selected `bsp_pwm.cpp`.
  - No graph contained `pnx_backends`, BMI088, DBUS RX or CAN/M2006
    validation sources.
- Public-header scan found no STM32 HAL type or peripheral handle in the
  retained CAN/SPI/USART/Flash/USB/PWM/indicator public headers.
- Template compatibility names stay HAL-free: `fdcan1/fdcan2` alias the F407
  CAN slots, while unsupported template PWM timer names fail closed.
- Independent static re-review found no remaining Critical or Important issue
  after USB lifecycle serialization/generation checks and USART RX ordering
  were corrected.

### Fresh Core hardware evidence

Observed on the connected DJI C-board on 2026-07-30:

- A clean-first 199-step `f407-debug` build produced
  `build/f407-debug/pnx_embedded.elf`, SHA-256
  `EDB72454568B5C719C622B5492BDCDF0B9AB61D1792343FD04D630E7A64A78CA`.
- Horco CMSIS-DAP v2 serial `482752132243` identified a Cortex-M4 r0p1 target,
  SWD DPIDR `0x2ba01477`, STM32 device ID `0x10076413`, and 1024 KiB Flash.
- OpenOCD program and read-back verification completed with `Verified OK`.
- Two reset/run attempts both reached the fresh ELF's `app_start` at
  `0x08006266`.
- After resume, PC samples were in the ThreadX PendSV handler.
  `DWT_CTRL=0x40000001` and `DWT_CYCCNT` advanced from `0x08331bdf` through
  `0x0e3fd50a` to `0x144ede84`.
- `GPIOH_ODR` changed from `0x00000000` to `0x00000800`, proving electrical
  activity on the Core smoke image's PH11 green-indicator output.
- The final command returned the verified Core image to reset/run and shut
  OpenOCD down.

This is a short, debugger-observed Core check. Physical LED appearance and
abnormal heat/motion were not remotely observed, and no long soak was run.
At this Core-check stage, optional peripherals were not started. The later
authorized servo, CAN/M2006 and BMI088 checks are recorded next. USB hardware
was not run because the current identity remains unassigned and intentionally
fails closed.

### Fresh optional hardware evidence

These tests used either the retained PWM validation image or task-local
disposable harnesses. The retired CAN/BMI088/DBUS release options were not
restored, and no task harness is part of a product source graph.

- PWM/servo:
  - Fresh ELF SHA-256:
    `BC298B1BB221DE881F1EB4E82C3CFE892C919300F86D30A2D039E212C30C3043`.
  - OpenOCD program/verify passed.
  - The bounded `1500 -> 1450 -> 1500 -> 1550 -> 1500 us` sequence completed
    5/5 steps with `faulted=0`, `output_enabled=0`; TIM1 `CR1=0`,
    `CCER=0` after completion.
  - Physical servo movement was not observed by Codex.
- CAN/M2006:
  - Task-local ELF SHA-256:
    `ED16B2CE2CE759D9A16E24E7B41BD93B554D4394D50F323D6370B82B3E548BA3`.
  - It linked the current direct `bsp_can.cpp` and unchanged template
    Motor/DJI Device sources.
  - OpenOCD program/verify passed. First-halt telemetry reported RX/TX
    `2328/1148`, last ID `0x203`, error/bus-off/drop/fault `0/0/0/0`,
    ten stable feedback samples, exactly 125 non-zero `+500` cycles, final
    current `0`, complete/faulted `1/0`; CAN1 ESR was `0`.
  - Physical M2006 movement was not observed by Codex.
  - A later explicit five-second follow-up retained the same `+500` command
    and safety guards. Clean-first task-local ELF SHA-256
    `C5A70AAE960321256CD63BFA57177BBAFB816EC8D9D67A67AA1719B17DD43259`
    programmed and verified successfully.
  - The measured command window was exactly 5000 ThreadX ticks at 1000 Hz
    (`start=20`, `end=5020`). Telemetry reported RX/TX `7712/3797`, ID
    `0x203`, error/bus-off/drop/fault `0/0/0/0`, 2501 control cycles, final
    current/speed `0/0`, and complete/faulted `1/0`; CAN1 ESR and the crash
    record were zero.
  - This proves the bounded command and safe final state. Physical rotation,
    sound, load, and temperature during those five seconds were not observable
    by Codex and require the user's confirmation.
- BMI088:
  - Task-local ELF SHA-256:
    `1DD5905EAD92F671B00E898493744A09C17C7875A5EE50B0CC327D3FABAF0DA0`.
  - It linked the current direct `bsp_spi.cpp` and unchanged template BMI088
    Device source; the heater was disabled.
  - SPI init, Device initialized, accel/gyro self-test, and reads all passed.
    Samples advanced `150 -> 251`, changing samples `149 -> 250`, all six axes
    changed, temperature was `32.875 C`, and `faulted=0`.
  - The template Device contains a hidden board dependency:
    `bmi088.cpp` references `GYRO_INT_Pin`, but this F407 IOC defines no
    BMI088 DRDY EXTI label. The polling-only task build used inert
    `GYRO_INT_Pin=0U` and never called the callback API. DRDY integration is
    therefore not validated and the Device is not fully drop-in on this board.
- DBUS:
  - Task-local ELF SHA-256:
    `8327445A9BF23D12B47B15DE9FB21C55595BE26A572BCBDE4EE56669D6846D08`.
  - It linked the current direct `bsp_usart.cpp`, unchanged template
    `msg.cpp`, and unchanged template `dr16.cpp`; the DR16 thread stack
    remained 768 bytes.
  - OpenOCD program/verify passed. The decisive four-second run reported
    heartbeat `434`, RX/update `310/310`, last length `18`,
    init/error/busy `0/0/0`, active source `1`, `offline=0`, and 272 motion
    samples.
  - Current and latched axes included `right_y=-0.286363631`,
    `left_x=0.0166666675`, and `left_y=-0.683333337`, proving live non-zero
    DR16 data reached the template parser.
  - One intervening 12-second run observed 225 short-frame/error/busy events
    and `offline=1`; repeating the exact artifact returned to healthy frames.
    No fault-handler entry, crash record, or reset was observed. Treat cable
    placement and signal quality as an attended follow-up, not a hidden pass.
- Every optional image was followed by successful Core program/verify. The
  board was left running Core ELF SHA-256
  `EDB72454568B5C719C622B5492BDCDF0B9AB61D1792343FD04D630E7A64A78CA`.

### Evidence boundary and next gate

`CORE_HARDWARE=PASS_SHORT_DEBUGGER_OBSERVED`,
`PHYSICAL_LED=NOT_OBSERVED`, `LONG_SOAK=NOT_RUN`,
`USB_HARDWARE=NOT_RUN_FAIL_CLOSED_UNASSIGNED_IDENTITY`,
`SERVO_MACHINE_EVIDENCE=PASS`, `CAN_M2006_MACHINE_EVIDENCE=PASS`,
`BMI088_POLLING=PASS_WITH_DRDY_LIMITATION`,
`PHYSICAL_ACTUATOR_MOTION=NOT_OBSERVED_BY_CODEX`,
`DBUS=PASS_WITH_INTERMITTENT_SIGNAL_OBSERVATION`, `H7=NOT_RUN`,
`REMOTE_CI=NOT_RUN`, and `FRESH_RECURSIVE_CLONE=NOT_RUN` for this working
tree.

Before publication:

1. Review and commit the `pnx_bsp` change, then push that exact commit.
2. Commit the parent changes and gitlinks; do not create new Device/Lib/Module
   branches.
3. Verify a fresh `git clone --recurse-submodules`.
4. Re-run the five retained presets and 23 host tests from that clone.
5. Run remote CI. Product-specific USB and optional-peripheral hardware
   validation remain separate attended gates.

## Context

- Date: 2026-07-28
- Repository: `C:\Users\USER\Desktop\RM\rm校內賽\2026\firmware\pnx_f4_minimal`
- Branch: `refactor/f407-minimal-architecture`
- Firmware commit: `342efc481b7153acc2815ed511139a6c8847ff66`
- Firmware artifact: `build/f407-debug/pnx_embedded.elf`
- Artifact SHA-256: `78C50EBFEE979EAD1332309FEBB7E74232310EA94F0B45C58CBCE34925688DF5`
- Operator: user, attended
- Pre-existing local item: untracked `testing.md`

## Objective

Validate the F407 Core Debug image on the DJI C-board without changing the
frozen P0 firmware baseline or exercising motor, servo, CAN, USB, IMU, or
other peripheral closures.

## Actual changes

No firmware, configuration, IOC, generated source, preset, submodule, or Git
history changes were made.

## Hardware and tools

- Target detected as STM32F407, Cortex-M4 r0p1, 1024 KiB Flash.
- Probe detected as Horco CMSIS-DAP v2, serial `482752132243`.
- OpenOCD 0.12.0 used with `interface/cmsis-dap.cfg` and
  `target/stm32f4x.cfg`.
- Arm GNU GDB from toolchain 13.3.Rel1 was used for breakpoint and register
  observations.
- An M2006 on CAN1 controller ID 3 and an A2 servo were physically connected,
  but the Core image sent no motor command and enabled no servo PWM.

## Verified evidence

- Flash programming completed and OpenOCD reported `Verified OK`.
- Reset PC was `0x08006214`, symbol `Reset_Handler`.
- Hardware breakpoints observed, in startup order:
  - `main`
  - `tx_application_define`
  - `app_start`
- Steady-state PC samples were in ThreadX `__tx_ts_wait`.
- DWT was enabled with `DWT_CTRL=0x40000001`.
- Steady DWT samples increased:
  - `0x5D564709 -> 0x675F0987`
  - final samples `0x2F06B20A -> 0x362A5DA1 -> 0x3C969AE7`
- After `reset run`, the target returned to ThreadX `__tx_ts_wait`; DWT was
  enabled again.
- GPIOH ODR changed from `0x00000000` to `0x00000800`, directly observing the
  PH11 green-indicator output transition.
- User observation:
  - `GREEN_LED=BLINKING`
  - `M2006=NO_MOVEMENT`
  - `A2_SERVO=NO_MOVEMENT`
- No HardFault, halt, unexpected reset, motor movement, or servo movement was
  observed during the attended smoke interval.
- OpenOCD was shut down after explicitly resuming the target; the MCU was left
  running independently.

## Failed attempts and resolution

- The initial OpenOCD attempt used `interface/stlink.cfg` and failed at probe
  open. Windows and pyOCD enumeration showed that the connected probe was
  CMSIS-DAP, not ST-Link. Using `interface/cmsis-dap.cfg` resolved the issue.
- GDB could not reopen the ELF through the repository's non-ASCII path. The
  ELF was copied to an ASCII temporary path and its SHA-256 was verified equal
  before use.
- A combined breakpoint run exceeded the F407 hardware-breakpoint budget
  because `HardFault_Handler` resolved to two locations. The startup points
  were then observed with a smaller sequential breakpoint set.

## Result

`ATTENDED-F407-CORE-HARDWARE-VALIDATION`: **PASS for the shortened attended
Core smoke scope**.

The planned 30-minute soak was shortened by user direction. Approximately a
few minutes of runtime plus a 90-second uninterrupted soak were observed.
Therefore the 30-minute stability criterion remains **NOT RUN** and must not
be cited as verified.

## Remaining scope

- Core 30-minute stability: `UNVERIFIED`
- USB CDC hardware: `UNVERIFIED`
- CAN RX/TX and motor control: `UNVERIFIED`
- Servo PWM: `UNVERIFIED`
- IMU, UART/DBUS, and concurrent-peripheral runtime: `UNVERIFIED`

## Rollback point

Git HEAD remains
`342efc481b7153acc2815ed511139a6c8847ff66`; the authorized USB changes are
uncommitted working-tree changes.

## USB CDC hardware follow-up

### Authorized configuration change

- The user explicitly authorized removal of the CMake rejection of VID
  `0x0483` and use of the existing RM26_F4 USB identity.
- At the initial identity-validation stage, the only production-source
  working-tree change was removal of that rejection block from
  `CMakeLists.txt`. The later separately authorized ZLP fix is recorded below.
- Configured identity:
  - VID `0x0483`
  - PID `0x5710`
  - manufacturer `STMicroelectronics`
  - product `Pikachu`
  - serial `LXYCAT`
- No IOC, generated source, preset, submodule, commit, remote, or tag was
  changed.

### Build and programming evidence

- Preset: `f407-usb-cdc-debug`
- Build: PASS with zero compiler/linker warnings.
- Pre-fix artifact: `build/f407-usb-cdc-debug/pnx_embedded.elf`
- Pre-fix artifact SHA-256:
  `05DD0BE20E8AC9C3D54E2E5D199D0958214ADCCCBE4B8DCEF5CF9BE6AB54D419`
- OpenOCD programming and verify: PASS.
- The target continued running ThreadX with DWT increasing and the PH11
  heartbeat active before the USB data cable was inserted.

### Host and CDC evidence

- Horco probe CDC enumerated as COM12.
- Target enumerated as COM17 with hardware ID
  `USB\VID_0483&PID_5710\LXYCAT`, product `Pikachu`, and repeated PnP status
  `OK`.
- CDC echo:
  - 1 byte: PASS
  - 16 bytes: PASS
  - 63 bytes: PASS
  - 64 bytes: FAIL; 64 bytes sent, zero bytes returned before host timeout
- Binary payload, 100-packet sequence, reconnect, reset/re-enumeration, and
  sustained-runtime tests were not run after the first boundary failure.
- Fault-register inspection after the failure showed CFSR, HFSR, DFSR, and
  AFSR all zero; the PC was in ThreadX `__tx_ts_wait`.

### Read-only diagnosis

- F407 carries USBX 6.1.10; its callback bulk-IN path sends an exact
  max-packet-size payload with equal transfer and host lengths and has no
  automatic ZLP support.
- `pnx_template` carries USBX 6.4.0, defines
  `UX_DEVICE_CLASS_CDC_ACM_WRITE_AUTO_ZLP`, and its callback bulk-IN path asks
  the stack to append a ZLP at this boundary.
- The observed 1/16/63-byte pass and exact 64-byte timeout are therefore
  consistent with a missing terminating ZLP. This is a strong causal
  inference, not yet a verified fix.

### Pre-fix USB result

`ATTENDED-F407-USB-CDC-HARDWARE-VALIDATION`: **PARTIAL PASS / BLOCKED**.

Enumeration and short CDC RX/TX echo passed. The required 64-byte boundary did
not pass, so USB CDC is not accepted and the CAN vertical slice must not be
started from this firmware state. No automatic source fix was made.

## P0 F407 USB ZLP boundary fix

### Authorization and scope

- The user explicitly authorized `P0-F407-USB-ZLP-BOUNDARY-FIX` and attended
  hardware revalidation.
- Production changes:
  - `boards/dji_c_board_f407/pnx_backends/usb_tx_completion.hpp`
  - `boards/dji_c_board_f407/pnx_backends/usb_backend.cpp`
- Test changes:
  - `tests/host/CMakeLists.txt`
  - `tests/host/usb_contract_host_tests.cpp`
- The fix adds a board-local asynchronous completion phase. An exact,
  successfully completed 64-byte CDC IN payload schedules one zero-length
  callback write before the common BSP receives the final completion.
- IOC, generated code, vendor USBX, common USB API, presets, submodules, CAN,
  motor, servo, IMU, commit history, remotes, and tags were not changed.

### TDD evidence

- Initial regression build failed because the completion policy did not
  exist.
- A deliberately non-ZLP scaffold then built, and
  `usb_f407_zlp_boundary` failed on the expected 64-byte action assertion.
- After implementation, the focused test passed.
- A second regression for a partial 64-byte completion failed before the
  guard and passed after requiring `actual == requested` before ZLP.
- Final host result: 12/12 PASS.

### Build evidence

- Fresh configure and build passed for:
  - `f407-debug`
  - `f407-release`
  - `f407-usb-cdc-debug`
  - `f407-usb-cdc-release`
- Compiler/linker warnings: zero.
- The Core source graph contains no `usb_backend.cpp` compile entry.
- Final artifact:
  `build/f407-usb-cdc-debug/pnx_embedded.elf`
- Final size: 2,281,708 bytes.
- Final SHA-256:
  `67E0989FB74EE5B0AC3B6C37550E52072892BE57198AE60BC0DF92D82F64B3EC`
- Final OpenOCD programming and verify: PASS.

### Hardware validation

- Final artifact enumeration:
  `USB\VID_0483&PID_5710\LXYCAT`, COM17, PnP status `OK`.
- Final artifact:
  - 64-byte binary echo: PASS.
  - 20/20 mixed echo sequence using 1/16/63/64-byte payloads: PASS.
  - SWD `SYSRESETREQ`, re-enumeration, and post-reset 64-byte echo: PASS.
- The immediately preceding implementation artifact additionally passed:
  - 1/16/63/64-byte boundary echo.
  - 64-byte binary echo.
  - 100/100 mixed packets, including 25 exact 64-byte packets.
  - Physical USB power/data cold disconnect and reconnect followed by a
    64-byte echo.
  - 15.005 seconds of continuous 64-byte bidirectional echo:
    15,909 packets and 1,018,176 bytes in each direction.
- The final source change after those extended checks only guards the
  partial-success error path; the final artifact was rebuilt, programmed,
  verified, and rechecked as listed above.

### Hardware/tool issue

- The first final-programming attempt reached the F407 but the Horco probe
  failed its physical SRST command with
  `CMD_DAP_SWJ_PINS failed / Unable to reset target`.
- A diagnostic attach with `reset_config none` read PC `0x0800F8BC`, proving
  SWD and target power were valid.
- Programming with physical SRST disabled and the STM32F4 target's Cortex-M
  `SYSRESETREQ` path completed and verified successfully.
- OpenOCD was shut down; final process count was zero.

### Final USB result

`P0-F407-USB-ZLP-BOUNDARY-FIX`: **PASS in the authorized shortened scope**.

The original exact-64-byte failure is reproduced, explained, regression
tested, and verified fixed on hardware. USB CDC no longer blocks beginning a
separately authorized CAN vertical slice. No commit or push was performed.

## F407 CAN1/M2006 vertical slice

Status: **PASS in the authorized attended scope**.

### Architecture changes

- Added the board-local STM32F407 bxCAN backend:
  `boards/dji_c_board_f407/pnx_backends/can_backend.cpp`.
- Reused the existing `pnx_bsp/can` contract without modifying it.
- Added the dedicated `PNX_ENABLE_CAN_M2006` application closure. Core and USB
  Debug Ninja graphs contain none of the CAN backend, CAN BSP, DJI motor, or
  CAN/M2006 application sources.
- The application uses the existing `m2006` and `djimotorhandler` classes
  directly. It does not restore `dji_motor_service`, an arm registry/token,
  USB arm commands, a factory, or a manager.
- Removed the remaining H7-specific default
  `bsp::can::bus::fdcan1` from
  `pnx_devices/motors/motor/include/motor.hpp`; the default is now the
  MCU-neutral `bsp::can::bus::none`.
- `pnx_devices` is a detached-HEAD submodule at
  `8a6783e63d77a15940aea8245bbe2eb13a2f2b11`. The one-line `fdcan1` correction
  is an uncommitted submodule working-tree modification; no submodule pointer
  was changed.
- A2 PWM was not initialized or enabled.

### Software validation

- Host tests: 18/18 PASS:
  - USB contract: 12.
  - CAN contract: 4.
  - M2006 template path: 1.
  - bounded one-shot sequence: 1.
- F407 CAN backend source acceptance: PASS.
- Embedded Debug builds:
  - Core: PASS, CAN/M2006 closure absent.
  - USB CDC: PASS, CAN/M2006 closure absent.
  - CAN/M2006: PASS, compiler/linker warnings zero.
- Non-generated/non-vendor source search found zero
  `bsp::can::bus::fdcan1`/`fdcan1` hits.

### Attended hardware validation

- Final CAN/M2006 ELF:
  `build/f407-can-m2006-debug/pnx_embedded.elf`
- Size: 1,572,932 bytes.
- SHA-256:
  `02C02F6A2B106B51F96E6A871DEFCC7DBAFE80747B6E44BC2C7AEB386BD3591D`
- OpenOCD programming and verify: PASS.
- First non-intrusive final telemetry snapshot:
  - heartbeat: 4,583
  - CAN RX: 9,310
  - CAN TX: 4,583
  - last ID: `0x203`
  - error/drop/fault epoch: `0/0/0`
  - non-zero pulse cycles: 125
  - final commanded current: 0
  - complete/faulted: `1/0`
- The operator observed one very small, brief M2006 movement on the immediately
  preceding implementation artifact using the same `+500`/250 ms sequence.
- An earlier symbol-read attempt halted the target and reproduced the known
  debugger-induced FIFO overflow. That observation was discarded. The final
  telemetry above was captured on the first halt after the final run and had
  zero CAN error/drop/fault.

### Safe restoration

- The board was restored to the normal Core Debug image.
- Restored Core ELF size: 1,418,560 bytes.
- Restored Core SHA-256:
  `0B34622FE890F2BDF665F26D1CEF5120D139A6D365F66CAEF6A11993C2F529F2`
- Core programming and verify: PASS.
- OpenOCD was shut down; no OpenOCD process remained.
- No commit, push, tag, remote change, IOC regeneration, generated-source
  edit, or Vault write was performed.

## F407 TIM1/PE11 servo vertical slice

Status: **PASS in the authorized attended scope**.

### Architecture changes

- Replaced the HAL-coupled shared PWM API with a board-neutral contract:
  opaque channel identity plus `start`, `stop`, `set_period_us`,
  `set_pulse_us`, and `set_duty`.
- The F407 board backend alone owns TIM1, channel 2, PE11 AF1, clock setup,
  HAL calls, and teardown.
- Added a dedicated `PNX_ENABLE_PWM_A2` application closure. Core, USB, and
  CAN Ninja graphs contain zero `pwm_backend.cpp` references.
- The board IOC and CubeMX-generated sources were not modified or regenerated.
- The test application has no motor layer, service, factory, registry, or
  persistent control interface.

### Software validation

- Host tests: 20/20 PASS, including the PWM contract and exact bounded servo
  sequence.
- Embedded Debug builds PASS with zero observed compiler/linker warnings:
  Core, USB CDC, CAN/M2006, and PWM/A2.
- PWM/A2 ELF SHA-256:
  `C3BF015E8354CD31369A031419CB676CFA33D92AAF425E32B61FE893D0E5EC5A`.

### Attended hardware validation

- OpenOCD program/verify/reset: PASS.
- Commanded sequence:
  `1500 -> 1450 -> 1500 -> 1550 -> 1500 us`, followed by a final center hold
  and output disable.
- Operator observation: the connected servo moved.
- Final telemetry:
  heartbeat `5`, step count `5`, pulse `0`, complete `1`, faulted `0`,
  output enabled `0`.
- Final register snapshot showed the TIM1 peripheral clock disabled after
  completion.

### Safe restoration

- The board was restored to the normal Core Debug image.
- Restored Core ELF SHA-256:
  `6D0CEFDCD1CE31DD4E0074813B4596912B2AFE6B4D6E278B0AE97EC83352964C`.
- Core programming and verify: PASS.
- No OpenOCD process remained.
- No commit, push, tag, remote change, IOC/generated edit, or Vault write was
  performed.

### Known integration boundary

- `pnx_bsp` is still at detached HEAD
  `f61e8ac8ac4b75d93b021ec32a8ecf56fc36a73b`; the PWM contract change is an
  uncommitted submodule working-tree modification.
- Dormant BMI088 code still names the former timer-specific PWM channels.
  Its board binding must be migrated when the BMI088 vertical slice begins;
  it was intentionally not pulled into this servo-only product graph.

## F407 shared-peripheral completion and provenance

Status: **software complete; BMI088 hardware PASS; DBUS hardware deferred by
operator direction**.

This section supersedes the earlier notes that the shared submodules were
dirty detached worktrees and that the BMI088 device driver still contained
timer-specific or H7-specific bindings.

### Architecture result

- `pnx_bsp` SPI, PWM, and Flash contracts are board-neutral. STM32 HAL types
  and calls exist only in C-board backends.
- The F407 SPI backend owns SPI1 Mode 3 and the PA7/PB3/PB4 pins. Board
  composition owns PA4 accelerometer CS and PB0 gyro CS.
- BMI088 register transport is independent of STM32 and has explicit injected
  bus and chip selects. The higher-level BMI088 driver also injects its
  optional heater PWM channel and data-ready attachment hook.
- The dormant WS2812 driver no longer hardcodes H7 `spi6`.
- F407 Flash sector geometry and HAL erase/program logic are board-local.
  No destructive Flash hardware test was run because this repository has no
  reserved test sector.
- H7 memory-bank labels `RAM_D1/D2/D3` were removed from shared code. Actual
  DMA buffers use `PNX_DMA_BUFFER`; ordinary objects use normal BSS; retained
  diagnostics use `.noinit`.
- The F407 USART backend uses generated USART handles only below the board
  boundary. The DR16 decoder parses the 18-byte wire format explicitly rather
  than relying on packed compiler bitfields.
- The automatic M2006 pulse image is no longer a normal preset. It is
  available only through the explicit
  `PNX_ENABLE_CAN_M2006_VALIDATION=ON` attended-validation switch.
- The default motor CAN bus is the board-neutral `bsp::can::bus::none`; no
  `fdcan1` reference remains in shared source.

### Formal submodule provenance

The following local branches and commits were created. They have **not** been
pushed:

- `pnx_bsp` `refactor/f407-portable-peripherals`:
  `899c0b491ec81e48ba8ed66ae90d451fb2bddc1b`
- `pnx_devices` `refactor/f407-portable-devices`:
  `e90601d8676922db954c90a06de97ef82d87bf01`
- `pnx_libs` `refactor/f407-memory-semantics`:
  `205217a42ac4e1e556e63f594eb7671f47e0e2b9`
- `pnx_modules` `refactor/f407-remoter-protocol`:
  `deaae6720a6051c0276b1ffbeb3fc0fc875456dd`

The parent integration records these four SHAs as gitlinks. The pre-existing
untracked `testing.md` remains outside Git.

### Final software validation

- Clean embedded builds: 7/7 PASS:
  `f407-debug`, `f407-release`, `f407-usb-cdc-debug`,
  `f407-usb-cdc-release`, `f407-pwm-a2-debug`,
  `f407-bmi088-debug`, and `f407-dbus-rx-debug`.
- Explicit attended CAN validation build: PASS using
  `PNX_ENABLE_CAN_M2006_VALIDATION=ON`.
- Host tests: 25/25 PASS, including USB, CAN, M2006 sequence, PWM, SPI,
  BMI088 transport, Flash contract, USART contract, and DR16 protocol.
- Compiler/linker warnings observed in the clean builds: zero.
- Core Ninja graph optional closure: none.
- Shared-source leakage scan found zero STM32 HAL types, `stm32h7`, `fdcan1`,
  old timer/SPI enum bindings, or `RAM_D1/D2/D3` names in
  `pnx_bsp`, `pnx_devices`, `pnx_libs`, and `pnx_modules`.
- `git diff --check`: PASS; Git only reported the repository's existing
  LF-to-CRLF checkout policy notices.

Final artifact hashes:

- Core Debug:
  `4736B2E1B0EFC47CD2A3D94CB10112EBA362DC57FADF38188BAB3B5A8C427BE1`
- BMI088 Debug:
  `F7E32A275CDE72D309700A97285DDAB1557A777BE3E524A35F0AFA4FF2535C02`
- DBUS RX Debug:
  `2E86C49C6FBF7AC54FDCD88F66611AF125CB6DCD9AE4D7F692854F055E01FF6A`
- Explicit CAN validation:
  `24B7165407F9EF2E8727CFE4AE1EC192CC3F8BDC59F6C586856A6ABAE596A458`

### Final BMI088 hardware evidence

- Final BMI088 artifact program/verify/reset: PASS.
- Runtime symbol:
  `demo::cboard::imu_bmi088::runtime` at `0x2000381C`.
- Snapshot A:
  heartbeat/sample/change `0x7DB/0x7DB/0x7DA`, IDs `0x1E/0x0F`,
  complete/faulted `1/0`.
- Snapshot B one second later:
  heartbeat/sample/change `0x865/0x865/0x864`, IDs `0x1E/0x0F`,
  complete/faulted `1/0`.
- All six raw axes changed between the two snapshots.

Result:
`SPI1 -> BMI088 device ID -> accelerometer/gyroscope raw data` is **PASS on
hardware** for the final source state.

### Final safe restoration and deferred evidence

- The board was restored to the final Core Debug artifact.
- Core program/verify/reset: PASS.
- Final Core sample: PC in ThreadX PendSV, DWT control/counter
  `0x40000001/0x1816AC86`, GPIOH ODR `0x800`.
- OpenOCD process count after resume/shutdown: zero.
- `DBUS_LIVE_FRAME`: **NOT RUN by explicit operator direction**.
- UART3/DBUS hardware behavior must not be presented as verified; only its
  backend build, USART contract, parser, and remote-module integration are
  verified.
- No remote push, tag, IOC edit, generated-code edit, destructive Flash test,
  CAN output, or PWM output was performed during this completion pass.

## 2026-07-29 architecture release-candidate cleanup

### Fact: scope and intent

This change is a narrow release-hygiene cleanup on top of the recorded F407
vertical-slice evidence. It does not alter IOC/generated files, startup,
linker, HAL runtime code, SPI1/BMI088 source, TIM1/PWM source, USB runtime
code, or application behaviour.

- Removed the unused H7-template SPI2/SPI6/TIM3/TIM12 capability parsing and
  generated SPI/PWM configuration from `configs/cmake/import_ioc.cmake` and
  `configs/cmake/generate_config.cmake`.
- Removed the unselected Flash public/backend closure and F4 Flash HAL source
  files from the default Core source graph. The board-local Flash backend is
  retained for a future explicit Flash slice; it remains unverified on
  hardware because no reserved destructive-test sector exists.
- Made every normal CMake preset explicitly set all optional application
  selectors. An attended CAN configure can therefore no longer silently
  persist into a subsequently selected normal Core/USB/PWM/BMI/DBUS preset.
- Updated the root README, repository rules, and board ownership note. The
  documents distinguish product images from attended validation closures and
  give one authoritative location for manual SPI1/TIM1 ownership.

### Fact: fresh software validation after the cleanup

On 2026-07-29, from a fresh CMake configure directory for each normal preset:

- 7/7 embedded builds PASS: `f407-debug`, `f407-release`,
  `f407-usb-cdc-debug`, `f407-usb-cdc-release`, `f407-pwm-a2-debug`,
  `f407-bmi088-debug`, and `f407-dbus-rx-debug`.
- The explicit attended CAN/M2006 image PASSed in its own binary directory
  `build/f407-can-m2006-debug` with all other optional closures OFF.
- Host tests: 25/25 PASS.
- `ninja -C build/f407-debug -t commands` contains none of
  `flash_backend`, `bsp_flash`, `usb_backend`, `can_backend`, `spi_backend`,
  `pwm_backend`, `bmi088`, or `usart_backend`.
- The removed generated-configuration identifiers have zero matches under
  `configs`; `git diff --check` PASSed.

### Inference: release status

The source tree is suitable for an **F407 architecture release candidate**:
the normal Core graph is now materially minimal, configuration authority is
clear, and optional hardware slices do not leak into it. The prior hardware
evidence remains relevant because this cleanup did not change the exercised
runtime code, but no new binary was programmed in this cleanup pass.

### Remaining release gates

- `DBUS_LIVE_FRAME=NOT_RUN`; DBUS remains software-validated only.
- The remote fresh-clone/submodule retrieval gate passed at
  `4a3a101a3d201b3d61cad71df730ed059559b7e0`; rerun it after any later
  source, submodule, preset, or toolchain-affecting commit.
- A public release still requires a repository-owner decision on LICENSE,
  NOTICE, and third-party distribution terms. No legal metadata is invented
  by this engineering cleanup.

### Fact: submodule clone URL correction

The initial remote fresh-clone attempt reached the parent and public
`pnx_bsp`, but failed before checkout because `pnx_devices`, `pnx_libs`, and
`pnx_modules` were recorded with unavailable `git@github.com:HKUSTGZ-ROBOMASTER-PNX/...`
SSH URLs. Their tested release remotes are the corresponding
`https://github.com/shiuhou/...` repositories, which already contain the
recorded F407 branches. `.gitmodules` is therefore corrected to these HTTPS
URLs before the final fresh-clone retry. This is release provenance repair,
not an architecture or firmware behaviour change.

### Fact: final remote reproducibility evidence

After the URL correction, a clean clone of
`refactor/f407-minimal-architecture@4a3a101a3d201b3d61cad71df730ed059559b7e0`
with `--recurse-submodules` completed from `origin`. It checked out exactly:

- `pnx_bsp@899c0b491ec81e48ba8ed66ae90d451fb2bddc1b`
- `pnx_devices@e90601d8676922db954c90a06de97ef82d87bf01`
- `pnx_libs@205217a42ac4e1e556e63f594eb7671f47e0e2b9`
- `pnx_modules@deaae6720a6051c0276b1ffbeb3fc0fc875456dd`

In that independent checkout, all seven normal embedded presets and the
isolated CAN attended image built successfully; `ctest` reported 25/25 host
tests PASS. The only observed transient was one HTTPS connection timeout while
cloning `pnx_modules`; Git retried automatically and checkout completed.

### Fact: final remote gate revalidation

On 2026-07-29, a second independent recursive checkout of
`origin/refactor/f407-minimal-architecture@957ce253a44d229b1aced496e8381956ace57bf0`
checked out the same four published submodule SHAs. From empty build
directories it configured and built all seven named presets, then built the
isolated `PNX_ENABLE_CAN_M2006_VALIDATION=ON` image. Native host configuration
used the root CMake project with `PNX_HOST_TESTS=ON`; `ctest` reported 25/25
PASS. No hardware was connected or operated.

Result: the remote fresh-clone gate is **PASS** for `957ce25`. A first
attempted host configuration against `tests/host` directly was invalid because
that directory is a subdirectory, not a standalone CMake project; it was not
a firmware build or test failure.

## 2026-07-29 team-internal RC documentation and local revalidation

### Actual changes

- Retained `HANDOFF.md` in the repository as the detailed evidence record.
- Moved the transient `testing.md` checklist to the parent `firmware/`
  directory; it is not tracked by this repository and is not release evidence.
- Updated `README.md` for team onboarding only:
  - defined `backend`, `closure`, and `fail-closed` at first use;
  - added a high-level repository tree and the actual CMake composition flow;
  - removed references to transient handoff/testing files and public-release
    legal metadata that are out of scope for this team-internal repository.

No firmware source, public interface, IOC/generated file, preset, submodule,
toolchain configuration, or hardware state was changed. Documentation commits,
remote synchronization, and tag state are recorded by Git history.

### Fresh local software validation

Working tree under test: `7f0129ad6a789e4cb466a51697d0bad2a10854d1` plus the
document-only `README.md` modification above. No hardware was connected or
operated.

- Fresh configure and build PASS for all normal presets using
  `cmake --fresh --preset <preset>` followed by
  `cmake --build --preset <preset> --parallel 4`:
  `f407-debug`, `f407-release`, `f407-usb-cdc-debug`,
  `f407-usb-cdc-release`, `f407-pwm-a2-debug`, `f407-bmi088-debug`, and
  `f407-dbus-rx-debug`.
- Fresh isolated CAN/M2006 configure and build PASS in
  `build/f407-can-m2006-debug`, with USB, PWM, BMI088, and DBUS selectors OFF
  and `PNX_ENABLE_CAN_M2006_VALIDATION=ON`.
- Fresh native host configure/build PASS in `build/host-release-check` with
  `PNX_HOST_TESTS=ON`; `ctest --output-on-failure` reported **25/25 PASS**.
- `ninja -C build/f407-debug -t commands` contained none of
  `usb_backend`, `can_backend`, `pwm_backend`, `spi_backend`, `bmi088`,
  `usart_backend`, `bsp_flash`, or `flash_backend`.
- Shared-layer leakage scan across `pnx_bsp`, `pnx_devices`, `pnx_libs`, and
  `pnx_modules` found no `stm32h7`, `fdcan`, `RAM_D[0-3]`, STM32 HAL, GPIO
  handle, or peripheral-handle identifiers.
- `git diff --check`: PASS. Git emitted only the existing LF-to-CRLF checkout
  notice for `README.md`.

### Remaining boundary

This revalidation is software-only. `DBUS_LIVE_FRAME=NOT_RUN`, the shortened
Core smoke scope, long soak, concurrent-peripheral runtime, and destructive
Flash testing retain their previously recorded evidence status. No new
hardware claim is made here.

## 2026-07-29 attended DBUS live-frame validation

### Actual source corrections

- `pnx_modules/remoter/include/remoter.hpp` increases only the DR16 ThreadX
  stack from 768 to 1536 bytes. The original image entered the registered
  ThreadX stack-error handler with the DR16 `TX_THREAD` control block as the
  context; its saved stack pointer was below its effective stack start.
- `demo/cboard/dbus_rx/dbus_rx.cpp` creates the message subscription in the
  already-running monitor thread rather than in `app_start()`. `app_start()`
  is invoked by `tx_application_define()`, where `msg::subscribe()` cannot
  wait on its ThreadX mutex. This is a lifecycle correction; no message-bus
  API or ThreadX policy was changed.

### Validation facts

- `f407-dbus-rx-debug` rebuilt successfully with RAM usage 51,696 B / 128 KiB
  (39.44%) and no observed build warnings.
- `ctest --test-dir build/host --output-on-failure`: **25/25 PASS**.
- The attended DBUS ELF SHA-256 was
  `C924BB2C35829997F9E8B8642F35CFF984B375C42F7106DF30F382B14C94FA67`;
  OpenOCD program and verify both passed on the STM32F407.
- With the receiver DBUS signal connected to USART3 RX (PC11) and common
  ground, live telemetry reached `offline=0`, `init_failed=0`, and the UART
  DMA reported 18-byte frames. The counters advanced from
  heartbeat/update `283436/44397` to `286047/46263`.
- Operator stick movement produced decoded values
  `right_x=-0.215151519` and `right_y=-0.506060600`; the remaining two axes
  were centered for that sample. This is physical evidence for receiver ->
  PC11 -> USART3 DMA -> DR16 decode -> message bus -> monitor telemetry.
- The board was restored to `f407-debug`; Core ELF SHA-256
  `78F5C5C7B8A9E920370EAE43973F913988F1B6838EE81594C945FB78497A5463` was
  programmed and verified successfully.

### Status

`DBUS_LIVE_FRAME=PASS`. This validation did not exercise USB, CAN, PWM,
BMI088, or Flash.

## 2026-07-29 release-hardening follow-up

### Actual changes

- `README.md` now records DBUS RX attended live-frame PASS, labels its preset
  as an attended validation image, and documents a local CMSIS-DAP/OpenOCD/GDB
  procedure. `.vscode/launch.json` remains intentionally local and ignored.
- `boards/dji_c_board_f407/README.md` records the composition rule discovered
  in DBUS validation: `app_start()` runs in `tx_application_define()` before
  application thread scheduling, so potentially suspending ThreadX calls
  belong in thread entries.
- `pnx_bsp/usart/src/bsp_usart.cpp` classifies a `restart_rx()` backend
  `busy` result as `busy_count`, not `error_count`, matching the existing
  transmit diagnostic semantics. The circular DMA may already be active when
  a DR16 timeout asks for a restart; this is contention, not a transport
  failure.
- `tests/host/usart_contract_host_tests.cpp` and its fake backend now cover
  that busy/error diagnostic distinction. The focused test was observed to
  fail before the production change and pass afterwards.
- `.github/workflows/f407-ci.yml` adds the smallest remote gate: recursive
  checkout, host CTest, F407 Core build, and F407 DBUS build. It uses only
  `actions/checkout@v7` with `contents: read` permission.

### Fresh local validation

- All seven normal embedded presets configured and built successfully:
  `f407-debug`, `f407-release`, `f407-usb-cdc-debug`,
  `f407-usb-cdc-release`, `f407-pwm-a2-debug`, `f407-bmi088-debug`, and
  `f407-dbus-rx-debug`.
- The isolated `PNX_ENABLE_CAN_M2006_VALIDATION=ON` image configured and
  built successfully with all other optional selectors OFF.
- Native host configuration/build succeeded; `ctest --test-dir build/host
  --output-on-failure` reported **25/25 PASS**. The focused USART contract
  test also passed after the red/green change.
- `git diff --check` passed. No hardware was operated during this follow-up.

### Remaining boundary

The GitHub Actions workflow has not run yet because it is uncommitted and
unpushed. Before publication, commit and push `pnx_bsp` and `pnx_modules`,
verify both remote SHAs, then commit/push the parent gitlinks and require one
successful remote CI run plus a fresh recursive clone gate.

## 2026-07-29 CI authentication and release gate

### Actual changes

- `.github/workflows/f407-ci.yml` now supplies the repository-managed
  read-only submodule token to both recursive `actions/checkout@v7` steps.
  This is required because the default `GITHUB_TOKEN` for the parent repository
  cannot read the private sibling submodules.

### Remote validation facts

- Parent commit `648da10df13acd93b16053eb2d2921539b8fe7a3` was pushed to
  `origin/refactor/f407-minimal-architecture`.
- GitHub Actions run `30448274030` completed successfully:
  <https://github.com/shiuhou/pnx_newtemplate_F4/actions/runs/30448274030>.
- Its `host-tests` job completed recursive checkout, configured and built the
  host target, and ran CTest successfully.
- Its `f407-builds` job completed recursive checkout, installed the GNU Arm
  toolchain, and built both `f407-debug` and `f407-dbus-rx-debug` successfully.

### Scope and remaining boundary

- This change only authenticates CI checkout. It does not alter firmware,
  public interfaces, generated code, CMake presets, submodule gitlinks, or
  hardware behavior.
- The CI secret value is intentionally not recorded in this repository.
- With the remote CI gate now passing, the next release action is to create an
  architecture RC tag from the final documentation commit. A production
  release still requires an owner decision on licensing and USB product
  identity.

The Vault was not modified.

## 2026-07-31 MyCar DR16/M2006 chassis planning handoff

### Repository state

- Repository/worktree:
  `C:\Users\USER\Desktop\RM\rm校內賽\2026\firmware\pnx_f4_mycar`
- Branch/HEAD:
  `mycar/f4@ed9a2271371c61001fd7440f413b542c2ba64218`
- Pre-existing working-tree state: untracked `demo/chassis_mecanum/`.
- All four submodules were uninitialized in this worktree at planning time.

### Actual changes

- Added the decision-complete implementation plan:
  `docs/superpowers/plans/2026-07-31-f4-mycar-dr16-m2006-chassis.md`.
- Added the engineering task packet under
  `.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/`.
- No chassis source, CMake product profile, configuration, submodule, remote,
  commit, push, firmware or hardware state was changed.

### Decision and planned architecture

- The user selected `pnx_f4_mycar`, four M2006s and a DR16 manual closed loop.
- Vehicle code will live under `vehicle/chassis`; no `pnx_application` or
  shared-interface change is planned.
- Pure kinematics, mapping, safety and velocity PI remain Host-testable.
  ThreadX, DR16, CAN and M2006 integration remains in a vehicle runtime
  adapter.
- Product geometry, speed, gains and current limits start at zero and fail
  closed. Non-zero motor output requires separate authorization.

### Validation and limitations

- `git` inspection confirmed the branch, HEAD, dirty state and uninitialized
  gitlinks.
- A native Host configure succeeded in `build/plan-baseline-host`; compilation
  failed on missing shared headers because the submodules were not initialized.
- Embedded build, flash, DR16 runtime, CAN runtime and physical motion were
  `NOT_RUN`.
- Commit and push remain unauthorized.

The Vault was not modified.
