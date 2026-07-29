# PnX DJI C-board STM32F407

這是以 `pnx_template@c025ad41b370faaeab128cf6389963a12e154a68`
為共用架構基線的 DJI C-board STM32F407 repository。它提供一個最小
Core image、一個可選 USB CDC image，以及明確隔離的硬體 validation
images；它不是把所有 template demo 編進同一個 firmware。

## 術語

- **Backend（板級實作）**：實作 board-neutral BSP contract 的板級程式碼；
  STM32 HAL handle、pin、IRQ 與 peripheral instance 應停留在這一層或
  CubeMX generated code。
- **Closure（功能閉包）**：為一個 firmware image 選入的一組
  application、BSP、board backend 與必要依賴 source。未選取的 closure
  不應進入該 image 的編譯與連結 graph。
- **Fail-closed（未確認即停用）**：必要設定或授權尚未確認時，功能拒絕
  啟動，而不是以猜測值或預設值繼續運作。

## Release 狀態

目前定位為 **F407 architecture release candidate**。

| Image / closure | 建置狀態 | 硬體證據 | 用途 |
| --- | --- | --- | --- |
| Core | 已驗證 | Core smoke PASS | 正常安全基線 |
| USB CDC | 已驗證 | 保留 USB CDC 證據 | 可選 CDC echo；未確認 identity 時 fail-closed |
| PWM A2 | 已驗證 | 保留 servo slice PASS | attended validation，不是產品 image |
| BMI088 | 已驗證 | 保留 BMI088 PASS | attended validation，不是產品 image |
| DBUS RX | 已驗證 | attended live-frame PASS | receiver DBUS → PC11 → USART3 DMA → DR16 → message bus 已實機驗證；validation image，不是產品 image |
| CAN/M2006 | 已驗證 | 保留 attended PASS | 僅 explicit attended validation；不提供 preset |

## 第一次使用：安全 Core 路徑

第一次接觸此 repository 時，只建置並燒錄 `f407-debug`。它選入 Core
smoke closure；不選入 USB、CAN、PWM、SPI、BMI088、DBUS 或 Flash
closure，因此是唯一的日常安全起點。不要以 PWM、BMI088、DBUS 或 CAN
validation image 作為第一次上板 image。

從空目錄取得固定 RC 與全部 submodule：

```powershell
git clone --branch f407-architecture-rc.1 --recurse-submodules `
  https://github.com/shiuhou/pnx_newtemplate_F4.git
cd pnx_newtemplate_F4
```

建置並以 CMSIS-DAP 燒錄：

```powershell
cmake --preset f407-debug
cmake --build --preset f407-debug
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg `
  -c "adapter speed 1000" `
  -c "program build/f407-debug/pnx_embedded.elf verify reset exit"
```

成功標準：C-board 綠燈（PH11）約每 500 ms 切換一次。這表示
`Reset_Handler → main → tx_application_define → app_start → ThreadX`
已進入 Core thread；Core 不會啟動 motor、servo、USB CDC、IMU 或 DBUS
application。若紅燈常亮或綠燈不閃，停止並先檢查 SWD、板卡供電與 Core
build／燒錄輸出；不要改選 attended validation image 來排除問題。

## 快速建置

需求：CMake 3.22+、Ninja、GNU Arm Embedded Toolchain
(`arm-none-eabi-gcc`、`arm-none-eabi-g++`)。

```powershell
cmake --preset f407-debug
cmake --build --preset f407-debug
```

可用 configure/build presets：

```text
f407-debug                 Core
f407-release               Core
f407-usb-cdc-debug         optional USB CDC
f407-usb-cdc-release       optional USB CDC
f407-pwm-a2-debug          attended PWM/A2 validation
f407-bmi088-debug          attended BMI088 validation
f407-dbus-rx-debug         attended DBUS RX validation
```

CAN/M2006 不屬於一般 preset。只有在已取得專門硬體操作授權、限制輸出
與監看設備狀態時，才可 configure：

```powershell
cmake -S . -B build/f407-can-m2006-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  '-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake' `
  -DPNX_ENABLE_CAN_M2006_VALIDATION=ON
cmake --build build/f407-can-m2006-debug
```

執行 shared contract 與 protocol host tests：

```powershell
cmake -S . -B build/host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

`tests/host` 是 root project 的子目錄，必須從 repository root configure；
不要直接以 `tests/host` 作為 CMake source directory。

## 本機 SWD 除錯

`.vscode/` 保持本機忽略，因 probe 路徑與序號屬於個人環境；不提交
`launch.json`。使用 CMSIS-DAP 時，可先啟動 OpenOCD：

```powershell
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg `
  -c 'adapter speed 1000' -c 'reset_config none'
```

另一個終端以對應 ELF 連線：

```powershell
arm-none-eabi-gdb build/f407-debug/pnx_embedded.elf
(gdb) target extended-remote 127.0.0.1:3333
(gdb) monitor reset halt
```

DBUS 實機驗證使用 `f407-dbus-rx-debug`，接收機 DBUS signal 接
PC11/USART3_RX 並共地；它不需要 microUSB。日常安全基線仍使用 Core
image。

## 架構與邊界

高階目錄：

```text
pnx_f4_minimal/
├─ CMakeLists.txt / CMakePresets.json / cmake/
│  └─ root build entry、preset 與 F407 GNU Arm toolchain
├─ configs/
│  ├─ boards/dji_c_board_f407/
│  └─ cmake/
│     └─ board policy、IOC import 與 build-local config generation
├─ demo/
│  ├─ app.cpp
│  └─ cboard/
│     └─ Core、USB、PWM、BMI088、DBUS、CAN/M2006 application closures
├─ pnx_bsp/
│  └─ board-neutral peripheral contracts
├─ pnx_devices/ / pnx_libs/ / pnx_modules/
│  └─ reusable device、utility、protocol 與 module code
├─ boards/dji_c_board_f407/
│  ├─ Core/ / AZURE_RTOS/ / USBX/
│  ├─ pnx_backends/
│  ├─ Drivers/ / Middlewares/
│  └─ startup_stm32f407xx.s / STM32F407XX_FLASH.ld
└─ tests/host/
   └─ shared contract 與 protocol host tests
```

執行層級：

```text
demo application
  → pnx_bsp public contract
  → DJI C-board backend
  → CubeMX / ThreadX / USBX / HAL / CMSIS
  → STM32F407 hardware
```

`pnx_bsp`、`pnx_devices`、`pnx_libs`、`pnx_modules` 保持共用 API；F407
HAL、startup、linker、ThreadX Cortex-M4 port、generated code 與 board
mapping 一律在 `boards/dji_c_board_f407`。`demo/app.cpp` 是唯一的
composition root，compile-time 強制只選一個 application closure。

根 CMake 只顯式列入所選 closure 的 source；Core 不含 USB、CAN、PWM、
SPI、BMI088、DBUS 或 Flash API closure。CAN/UART 的 CubeMX generated
initialization 保留在 board support，表示硬體 capability，並不代表產品
application 已啟動對應 BSP service。

### CMake flow

```text
root CMakeLists.txt
├─ PNX_HOST_TESTS=ON
│  └─ 建立 native C++ host-test project → add_subdirectory(tests/host) → return
└─ embedded build
   ├─ 宣告並檢查 USB / CAN / PWM / BMI088 / DBUS application selectors
   ├─ 指定 F407 IOC、params.json 與 robot.json
   ├─ generate_config.cmake 產生 build-local config.hpp / robot_config.hpp
   ├─ 依 selector 顯式組成 PNX_EMBEDDED_SOURCES 與 include directories
   ├─ add_executable(pnx_embedded)
   └─ add_subdirectory(boards/dji_c_board_f407/cmake/stm32cubemx)
      ├─ stm32cubemx interface target
      ├─ STM32_Drivers object target
      ├─ ThreadX object target
      └─ USB enabled 時才建立並連結 USBX object target
```

`PNX_ENABLE_USB_CDC` 是 USB 產品功能的唯一 user-facing selector；
`PNX_F407_USB_ENABLED` 只是在 root CMake 內由它推導，供 board CMake
控制 PCD、USBX、descriptor、IRQ include 與 source closure，不是第二個
設定來源。

## Configuration authority

- `boards/dji_c_board_f407/dji_c_board_f407.ioc`：CubeMX 管理的 clock、
  generated CAN/UART/USB、一般 GPIO 與 BMI088 chip-select 的安全開機狀態。
- `configs/boards/dji_c_board_f407/*.json`：產品 policy 與 IOC-derived
  CAN/USART/USB binding 的輸入；CMake 產生 build-local `config.hpp`。
- `boards/dji_c_board_f407/pnx_backends`：F407 runtime backend。SPI1/BMI088
  與 TIM1_CH2/PE11 PWM 是刻意手寫的 board resource，不由 IOC 產生。

完整 pin、lifecycle 與 regeneration 規則見
[Board README](boards/dji_c_board_f407/README.md)。不可在未完成專門
CubeMX review 與對應硬體 revalidation 前，將手寫 SPI1/TIM1 ownership
移入 IOC。

## USB identity

USB CDC 的選擇權只有 `PNX_ENABLE_USB_CDC`。IOC 僅說明 USB 硬體能力。
預設 VID/PID/serial 未配置，F407 USB backend 必須 fail-closed；只有取得
授權 identity 後才設定 `PNX_USB_DEVICE_IDENTITY_CONFIRMED=ON` 與完整
VID/PID/manufacturer/product/serial。

## Repository 與發布規則

四個 `pnx_*` 目錄是 submodule，不可只發布 parent gitlink。發布順序是：

```text
push pnx_bsp / pnx_devices / pnx_libs / pnx_modules commits
→ push parent commit
→ verify fresh clone --recurse-submodules
→ build all presets and run host tests
```
