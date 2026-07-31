# PnX DJI C-board STM32F407

這個 repository 是 DJI C-board（STM32F407）的最小 firmware 基礎模板。
它負責 F407 board support、共用 submodule 整合、安全 Core image、可選
USB CDC image，以及隔離的 PWM/A2 驗證 image；正式 chassis、gimbal、
shooter 等車組 application 應放在各自 repository。

## mycar/f4 vehicle ownership

This mycar/f4 branch is the vehicle-specific composition.
F407 Board/HAL ownership remains unchanged.
Vehicle code belongs under vehicle/.
Shared pnx_* gitlinks and public APIs remain upstream-owned.
Normal Core/USB/PWM images remain zero-motor images.
Non-zero motor output remains separately authorized.

本 repository 只實作 F407；不在這裡加入、修改或驗收 H7 source。
F407 與 H7 的 BSP 私有實作及發布流程彼此獨立；兩者的 public API 形式
協調屬於另一個跨 repository 任務，不是本 RC 的發布依賴。

## 術語與核心規則

- **BSP contract（板級支援契約）**：`pnx_bsp/*/include` 中不含 MCU/HAL
  型別的公共 API。Application、Device 與 Module 只依賴這些 header。
- **Backend（後端／板級實作）**：舊結構中指 public BSP function 之後的
  另一組 `detail::backend_*` 轉發函式。本 repository 已移除這種無額外
  語意的獨立轉發層；F407 source 直接定義 public `bsp::*` symbols。
- **Closure（功能閉包）**：一個 firmware image 實際選入的 application、
  BSP implementation 與必要依賴 source。未選取的 closure 不得進入該
  image 的 compile/link graph。
- **Fail-closed（未確認即停用）**：必要設定或授權未確認時，功能拒絕
  啟動，不使用猜測值繼續執行。USB identity 即採此策略。

固定邊界如下：

```text
Application / Device / Module
              │
              ▼
      public pnx_bsp header
              │
              ▼
boards/dji_c_board_f407/bsp/bsp_*.cpp
              │
              ▼
       STM32F4 HAL / CMSIS
```

每個 firmware target 只連結一份 public symbol implementation。沒有
runtime registry、factory、service locator、function-pointer table，亦
沒有用 runtime `if (F407/H7)` 選 MCU。

## 目前可建置範圍

| Image | Preset | 目前本地軟體驗證 | 用途 |
| --- | --- | --- | --- |
| Core Debug | `f407-debug` | RC2 local PASS，RAM 49,792 B／Flash 23,736 B | 日常安全起點 |
| Core Release | `f407-release` | RC2 local PASS，RAM 49,792 B／Flash 15,188 B | 最小 release build |
| USB CDC Debug | `f407-usb-cdc-debug` | RC2 local PASS，RAM 66,040 B／Flash 67,952 B | 可選 USB CDC |
| USB CDC Release | `f407-usb-cdc-release` | RC2 local PASS，RAM 66,000 B／Flash 40,056 B | 可選 USB CDC |
| PWM/A2 Debug | `f407-pwm-a2-debug` | RC2 local PASS，RAM 49,864 B／Flash 27,656 B | attended hardware validation，不是產品 composition |

以上數字已在更新 shared pins 後的本地 working tree 重新 fresh
build/link；working tree 的 parent base 是
`d9d85acdca5a82a330762e02c7dd9b0f580d54bb`，但本次修改尚未 commit，
因此不是新 release commit 或 RC2 tag 已發布的宣稱。五個 preset 均為
0 compiler/linker warnings、恰有一個 `app_start`，且沒有重複的
strong `bsp::*` definition。

BMI088、DBUS RX 與 CAN/M2006 validation closure 已撤下。它們先前依賴
非 template 的 Device/Lib/Module API；在三個 submodule 回到 template
gitlinks 後，本 repository 不再提供其 preset、demo、CI build 或目前
支援宣稱。若仍傳入下列舊選項，CMake 會明確拒絕：

```text
PNX_ENABLE_BMI088
PNX_ENABLE_DBUS_RX
PNX_ENABLE_CAN_M2006_VALIDATION
```

CAN、SPI 與 USART 的 public contract 與 F407 direct source 仍存在，供
之後的 application repository 使用；它們目前沒有被連結成 public BSP
Core closure，也沒有在此 repository 宣稱新的硬體 validation。Flash
source 也保留，但在 linker 保留資料 partition 前不屬於任何受支持 image。
Host fake 測試驗證的是公共 API contract，不等於 F407 HAL runtime 或
硬體通過。

## 第一次使用

需求：CMake 3.22+、Ninja、GNU Arm Embedded Toolchain
（`arm-none-eabi-gcc`、`arm-none-eabi-g++`）。

取得 parent 與固定 submodules：

```powershell
git clone --branch f407-architecture-rc.2 --recurse-submodules `
  https://github.com/HKUSTGZ-ROBOMASTER-PNX/pnx_template.git pnx_f4_minimal
cd pnx_f4_minimal
```

先建置 Core：

```powershell
cmake --preset f407-debug
cmake --build --preset f407-debug
```

若已取得燒錄授權，可用 CMSIS-DAP：

```powershell
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg `
  -c "adapter speed 1000" `
  -c "program build/f407-debug/pnx_embedded.elf verify reset exit"
```

Core 的既有 attended smoke 標準是 PH11 綠燈約每 500 ms 切換一次，
代表 startup、`main`、ThreadX composition 與 Core thread 已執行。Core
不會啟動 USB、PWM、CAN BSP service、SPI/BMI088、DBUS 或 Flash 操作。
歷史硬體證據與限制見 [HANDOFF.md](HANDOFF.md)；本次架構整理沒有操作
硬體。

執行 native host tests：

```powershell
cmake -S . -B build/host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

`tests/host` 必須由 repository root configure，不是獨立 CMake project。

## 高階目錄

```text
pnx_f4_minimal/
├─ CMakeLists.txt / CMakePresets.json
│  └─ host/embedded 分流與 compile-time image composition
├─ cmake/
│  └─ GNU Arm toolchain
├─ configs/
│  ├─ boards/dji_c_board_f407/
│  └─ cmake/
│     └─ IOC-derived config 與 build-local header generation
├─ demo/
│  ├─ app.cpp
│  └─ cboard/
│     ├─ board_smoke/
│     ├─ usb_cdc/
│     └─ pwm_a2/
├─ pnx_bsp/
│  └─ board-neutral public BSP contracts
├─ pnx_devices/ / pnx_libs/ / pnx_modules/
│  └─ template-pinned reusable layers
├─ boards/dji_c_board_f407/
│  ├─ bsp/
│  │  ├─ bsp_can.cpp
│  │  ├─ bsp_diagnostics.cpp
│  │  ├─ bsp_flash.cpp
│  │  ├─ bsp_indicator.cpp
│  │  ├─ bsp_pwm.cpp
│  │  ├─ bsp_spi.cpp
│  │  ├─ bsp_usart.cpp
│  │  ├─ bsp_usb.cpp
│  │  └─ board-private support headers / fault_handlers.S
│  ├─ Core/ / AZURE_RTOS/ / USBX/
│  ├─ Drivers/ / Middlewares/
│  └─ startup_stm32f407xx.s / STM32F407XX_FLASH.ld
└─ tests/host/
   └─ direct fake implementations與 public contract tests
```

Device 與 Module 可以知道 `bsp::can`、`bsp::spi`、`bsp::usart` 等公共
contract，但不可知道 `hcan1`、`huart3`、GPIO pin、DMA stream、IRQ 名稱
或 `HAL_*`。這些硬體細節只能出現在 board directory。

為了讓未分支的 template Device/Module 直接編譯，公共 contract 保留
下列相容名稱：

- CAN 的 `fdcan1`/`fdcan2` 是 F407 `can1`/`can2` 的邏輯別名；
- SPI 的 `spi2` 對應 F407 IMU 邏輯 slot（board 內實際是 SPI1），
  `spi6` 在本板未配置；
- PWM 的 `tim3_ch4`/`tim12_ch2` 可通過編譯，但在 C-board 映射為
  unsupported slot，操作會 fail-closed，不會誤驅動 TIM1_CH2/PE11。

這些只是 template consumer API 相容識別字，不是 H7 source、H7 HAL
支援或硬體驗證宣稱。

## CMake flow

```text
root CMakeLists.txt
├─ PNX_HOST_TESTS=ON
│  ├─ native compiler
│  ├─ tests/host/fake_*.cpp 直接定義 public bsp::* symbols
│  └─ CTest
└─ embedded build
   ├─ 讀取 F407 IOC + params.json + robot.json
   ├─ 產生 build/<preset>/generated/config.hpp
   ├─ 加入共同 Core source
   ├─ compile-time 選擇
   │  ├─ no optional selector → board_smoke + Core BSP
   │  ├─ PNX_ENABLE_USB_CDC → bsp_usb.cpp + usb_cdc
   │  └─ PNX_ENABLE_PWM_A2 → bsp_pwm.cpp + pwm_a2
   ├─ 由實際 source list 推導 generated peripheral init guards
   ├─ add_executable(pnx_embedded)
   └─ board CMake 連結 CubeMX / HAL / ThreadX
```

`PNX_ENABLE_USB_CDC` 與 `PNX_ENABLE_PWM_A2` 互斥。這些 selector 是
isolated image composition，不應演變成 chassis/gimbal/shooter 等產品
子系統選單。正式 robot application 應在另一個 repository 組合所需
Device 與 Module。

CubeMX 仍可能把 `can.c`、`usart.c` 等 generated capability source 放進
Core build；這不代表 `bsp_can.cpp` 或 `bsp_usart.cpp` 已啟動或連結。
root CMake 以同一份 `PNX_EMBEDDED_SOURCES` 判斷 Direct BSP source 是否
實際被選入，並據此 guard `main.c` 的 CAN／USART generated init call。
目前五個 RC2 image 都沒有選入這兩個 source，因此不執行 CAN1、CAN2 或
USART1/3/6 初始化；未來選入對應 Direct BSP source 的 image 才會啟用。

## Configuration authority

- `boards/dji_c_board_f407/dji_c_board_f407.ioc`：clock、generated
  CAN/UART/USB capability、一般 GPIO 與開機安全狀態。
- `configs/boards/dji_c_board_f407/*.json`：board policy 與 public
  contract binding；輸出只寫到 build-local generated headers。
- `boards/dji_c_board_f407/bsp/`：F407 direct implementation，持有 HAL
  handle、pin、IRQ、DMA 與 peripheral instance。

SPI1/BMI088 與 TIM1_CH2/PE11 目前是手寫 board resource；完整 ownership
與 CubeMX regeneration 規則見
[Board README](boards/dji_c_board_f407/README.md)。

## USB identity

`bsp::usb::init(config)` 採非阻塞、異步 startup。返回 `ok` 只代表 config
與 callback ownership 已接受、必要 ThreadX resources 已建立且 startup
worker 已安排；不代表已枚舉、host 已連接或 CDC ready。只有 CDC transport
實際可用時 `connected()` 才為 true。startup failure 進入可觀察的 fault
狀態；`write()` 在未 ready、斷線或 bounded queue 滿時明確返回非 `ok`，
不會無限等待或累積。

完全相同的 config 可重複初始化；不同 callback、user context 或不相容
參數會被拒絕，不會靜默改寫 ownership。F407 callback 的 thread context、
buffer lifetime 與 disconnect 行為見
[Board README](boards/dji_c_board_f407/README.md)。

USB closure 預設為 fail-closed：VID/PID 是 `0x0000`，serial 是
`UNASSIGNED`。只有取得團隊授權 identity 後，才可同時設定：

```text
PNX_USB_DEVICE_IDENTITY_CONFIRMED=ON
PNX_USB_DEVICE_VID=<authorized VID>
PNX_USB_DEVICE_PID=<authorized PID>
PNX_USB_DEVICE_MANUFACTURER=<authorized string>
PNX_USB_DEVICE_PRODUCT=<authorized string>
PNX_USB_DEVICE_SERIAL=<stable authorized serial>
```

## RC2 deferred capability matrix

| Capability | RC2 status | Required before use |
| --- | --- | --- |
| Flash | `UNSUPPORTED_UNTIL_RESERVED_PARTITION` | Linker 必須保留資料 partition；`layout()` 只暴露該區；erase/program 必須拒絕 firmware address。 |
| BMI088/AHRS | `NOT_IN_RC2_PRODUCT_GRAPH` | 保留 template Device source；正式使用前處理 SPI bootstrap、transaction ownership、EXTI/DRDY mapping 與 ThreadX concurrency；不建立 F407 Device fork。 |
| Public typed USB adapter | `UNSUPPORTED_UNTIL_CONTRACT_FIX` | RC2 只支持 raw config/API；正式使用 `make_config<T>()` 前處理 alignment、object lifetime、trivially-copyable 與 caller-owned context。 |

這三項是使用前條件，不是 Direct BSP 架構、Core、raw USB API 或 PWM
closure 的失敗。

## Submodule 與發布邊界

目前 Device/Lib/Module 已更新到
`pnx_template@cf6577765358822a1bc57c1ea17fe65a795ceb62` 的 exact commits：

```text
pnx_devices  2349cc108c9ed477ccdcd700e802ea888975cdfd
pnx_libs     e7c3e7a2b9d825586ab3e0c413877180c4295df8
pnx_modules  8ba925b60b11fec511a57622c199b57bb23f8f4e
```

這三個 submodule 不需要為 F407 另開長期 branch。F407 所需的公共 BSP
contract 變更在 `pnx_bsp`，F407 HAL 實作在 parent repository 的
`boards/dji_c_board_f407/bsp/`。

最新 `pnx_modules` 內含 PS2 receiver 與 Tactical AHRS source，但 RC2
image graph 不選入兩者。F407 config generator 已提供 disabled-by-default
的 PS2 symbols，Direct USART BSP 也提供 board-neutral line configuration
contract，讓之後的 application repository 不必恢復 backend。BMI088
source 仍需要正式的 F407 `GYRO_INT_Pin`／EXTI mapping，因此
BMI088/AHRS 的 deferred status 不變。

Host suite 會直接編譯 template 的新 BMI088、LK9025、PID 與 PS2 public
API，並驗證 F407 config generation；SPI/PWM/USART contract tests 也會
檢查相容入口。這只能證明 API 與 fail-closed software contract，不能
代替實機驗證。

RC2 發布必須先讓新的 F407 `pnx_bsp` SHA 可由官方 remote 取得，再提交
parent gitlink。只有 remote recursive clone 重跑五個 presets、完整
host suite、F407 source graph 與 symbol gates 全部通過後，才能建立
annotated tag；H7 regression 不屬於此流程。
