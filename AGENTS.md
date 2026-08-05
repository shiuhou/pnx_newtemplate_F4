# Pure-F407 Repository Rules

This repository contains the DJI C-board STM32F407 baseline plus the
vehicle-specific MyCar chassis composition. Its product graphs are Core,
optional USB CDC, and the fail-closed MyCar DR16/four-M2006 image. PWM A2 is
an isolated validation closure, not a product profile. BMI088 and standalone
DBUS validation evidence are historical and are not current build surfaces.

## Vehicle-specific branch ownership

The `mycar/f4` lineage and its `feat/*` branches own vehicle composition.
F407 Board/HAL ownership remains unchanged.
Vehicle code belongs under vehicle/.
Shared pnx_* gitlinks and public APIs remain upstream-owned.
Normal Core/USB/PWM images remain zero-motor images.
Non-zero motor output remains separately authorized.

- Use `pnx_template@cf6577765358822a1bc57c1ea17fe65a795ceb62` as the
  shared-architecture baseline.
- Keep F407 compiler, generated code, Board, startup, linker, ThreadX, HAL,
  and resource mapping below the Board/platform boundary.
- Keep shared public APIs free of STM32 HAL types, MCU-family symbols,
  other-MCU memory-bank labels, and board handle names.
- Keep F407 direct BSP implementations in the F407-specific
  `pnx_bsp/*/src` directories. Those sources define public `bsp::*` symbols
  directly; do not move them back into a parent-only BSP tree or reintroduce
  a `detail::backend_*` forwarding layer.
- IOC/CubeMX owns generated resources. The documented manual board resources
  are SPI1/BMI088 and TIM1_CH1/2/3/4 on PE9/PE11/PE13/PE14 for PWM; do not
  transfer ownership without a reviewed CubeMX task and hardware revalidation.
- Core must not acquire an optional public BSP implementation or consumer
  closure. Generated `can.c`/`usart.c` may remain compiled as dormant Board
  capability, but root CMake must derive their `main.c` init guards from the
  actual Direct BSP source selection; Core must not execute those init calls.
- Add a validation selector, manager, telemetry or test framework, or new
  abstraction only when it is required by an existing `pnx_template`
  contract, a proven F407 hardware difference, or a current product feature.
  If none applies, do not add it.
- Validation closures must not become the product subsystem-selection
  architecture. Production robot applications belong in separate
  vehicle-specific repositories, which compose the accepted shared submodules
  and F407 Board baseline without expanding this repository into a product
  application.
- Motor output remains zero in normal images. Non-zero motor output and all
  hardware operation require separate explicit authorization.
- USB identity remains unassigned until explicitly approved, and controller
  start must remain fail-closed.
- Do not push, modify remotes, tag, publish, regenerate CubeMX, or operate
  hardware without explicit authorization.

## Vehicle feature integration

- Treat `mycar/f4` as the vehicle integration mainline. Develop chassis, arm,
  MaixCam and later vehicle features on isolated feature branches or
  worktrees; do not use a feature branch as the permanent vehicle mainline.
- Before a completed feature is accepted, synchronize it with the latest
  `mycar/f4` and retain only that feature's changes. Do not re-import stale
  chassis, shared-module or BSP snapshots from the feature branch.
- Acceptance requires the full Host test suite, the affected F407 preset or
  camera test suite, and the hardware checks required by that feature. Record
  the exact validated commit and evidence before integration.
- After acceptance and separate explicit authorization, tag the exact
  validated feature commit (for example `arm-baseline-v1`), merge it into
  `mycar/f4`, and retain or rename the completed branch under `archive/*`.
- A shared Git branch does not imply a single firmware image. Chassis and arm
  remain separate C-board presets/images; MaixCam remains camera software plus
  its explicitly selected STM32 communication closure.

## Default engineering mode

除非使用者明確要求升級或降級，工程任務一律採用 STANDARD 模式：

- `SUBAGENT = OFF`：主 agent 自行實作與自我 review，不派獨立 subagent
  進行「實作代理＋審查代理」分工；只有任務確實可拆成兩個以上互不依賴的
  大型工作，或使用者明確指定 `subagent-driven` 時才啟用。
- `FULL_TASK_PACKET = OFF`：只建立精簡版 task 記錄，包括 objective、
  baseline、變更與驗證結果；不預設產出完整九件式 task packet。
  `source-map`、`system-understanding`、`experiment-record` 等只在確有需要時
  建立。
- `HANDOFF_UPDATE`：只在使用者明確要求，或任務屬於重大里程碑時更新。
  重大里程碑包括 public contract、safety/fail-closed 狀態、release/RC gate
  或跨 repository 邊界變更。
- `HANDOFF_SCOPE`：只有重大或實質建設性的進度才放入 `HANDOFF.md`；例如
  可交接的功能完成、架構／安全決策、跨 repository 變更、正式驗證結論或
  release gate。探索、微小修正與一般日常進度不更新 handoff。
- `CURRENT_TASK_CHECKPOINT`：若有未 commit 的工作預期跨 session 繼續，或
  使用者要求暫停／明天續作，更新 `.codex/CURRENT_TASK.md`。內容只保留
  objective、目前狀態、已驗證項目、下一步與禁止事項；開始續作非全新任務前，
  若此檔存在必須先讀取。已 commit 的完成工作以 Git history 為主要進度來源，
  不需要 checkpoint。
- `FULL_PRESET_MATRIX`：目前六個 F407 preset 的完整 clean rebuild、ELF 與
  SHA 核對只在 release 前執行；日常開發只 build 本次變更實際影響的
  preset。
- `FOCUSED_TESTS = ALWAYS`：每次改動都執行與該次變更直接相關的測試。
- `HOST_FULL_TEST`：全部 Host CTest 只在準備 commit 前執行一次。
- 硬體操作、commit、push 與跨 workspace 寫入一律需要使用者明確授權；
  此要求不受工程模式影響。

使用者可以隨時明確切換模式：

- 「用精簡模式」：進一步降級；不建立 task packet、不更新 `HANDOFF.md` 或
  `VAULT_UPDATE.md`，只做必要修改、聚焦測試與一次相關 build。
- 「用發布模式」、`release-grade` 或 `subagent-driven`：升級為完整流程，
  包括完整 task packet、獨立 subagent review、完整 preset matrix，以及
  `HANDOFF.md`／`VAULT_UPDATE.md` 更新。

未明確指定模式時，一律視為 STANDARD。

---

## Code-writing discipline

無論目前是哪種 engineering mode，寫程式碼時一律遵守以下四條（源自
Andrej Karpathy 對 LLM coding 常見問題的觀察）：

- **Think Before Coding**：遇到不確定的地方，明講你的假設，不要默默選一種
  解讀就往下做。如果有多種合理解讀，攤出來問，不要用猜的。如果有更簡單
  的做法，主動提出來，不要照單全收。
- **Simplicity First**：只寫解決問題所需的最少程式碼。不加沒被要求的
  彈性或可配置性。硬體、資料完整性、輸出安全與 fail-closed 情境不視為
  「不可能發生」，仍須按既有安全規則處理。若能在不擴大風險、不削弱可讀性、
  測試或安全保證的前提下明顯簡化，先提出重寫建議。
- **Surgical Changes**：只碰這次任務必須碰的地方。不要「順手」改動、
  重構或美化旁邊沒被要求的程式碼、註解或格式，即使你覺得原本寫法不好。
  每一行改動都應該能對應回這次的具體要求。只有在你自己的改動導致某段
  程式碼變成孤兒（未使用的 import/變數/函式）時才移除；既有的、跟這次
  無關的死碼只需要提出來，不要動手刪。
- **Goal-Driven Execution**：把任務轉成可驗證的成功標準，做完要能證明
  達成，而不是憑感覺說「應該可以了」。

### 與 Default engineering mode 的橋接

`Goal-Driven Execution` 要求驗證，但驗證的嚴謹程度跟著目前的 engineering
mode 走，不是固定套用完整 TDD：

- `STANDARD`（預設）：用跟這次改動直接相關的聚焦測試證明成功標準達成，
  不需要為每一行都跑一次完整 red-green-refactor。
- 精簡模式：一次快速手動檢查或跑最相關的一項測試即可。
- 發布模式／`release-grade`／使用者明確指定 `subagent-driven`：比照現有
  完整流程，要求 test-first 的完整 TDD loop。

`Simplicity First` 與 `Surgical Changes` 兩條不受 engineering mode 影響，
任何模式下都必須遵守——它們限制的是「這次改動本身該有多大」，跟「要不要
包一層流程」是兩件事。
