# PnX DJI C-board STM32F407

這是以 `pnx_template@c025ad41b370faaeab128cf6389963a12e154a68`
為共用架構基線的 DJI C-board STM32F407 repository。它提供一個最小
Core image、一個可選 USB CDC image，以及明確隔離的硬體 validation
images；它不是把所有 template demo 編進同一個 firmware。

## Release 狀態

目前定位為 **F407 architecture release candidate**。

| Image / closure | 建置狀態 | 硬體證據 | 用途 |
| --- | --- | --- | --- |
| Core | 已驗證 | Core smoke PASS | 正常安全基線 |
| USB CDC | 已驗證 | 保留 USB CDC 證據 | 可選 CDC echo；未確認 identity 時 fail-closed |
| PWM A2 | 已驗證 | 保留 servo slice PASS | attended validation，不是產品 image |
| BMI088 | 已驗證 | 保留 BMI088 PASS | attended validation，不是產品 image |
| DBUS RX | 已驗證 | `DBUS_LIVE_FRAME=NOT_RUN` | software-only；不可宣稱實機完成 |
| CAN/M2006 | 已驗證 | 保留 attended PASS | 僅 explicit attended validation；不提供 preset |

詳細、帶條件的證據與未驗證範圍見 [HANDOFF.md](HANDOFF.md)。

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
f407-dbus-rx-debug         DBUS software-validation image
```

CAN/M2006 不屬於一般 preset。只有在已取得專門硬體操作授權、限制輸出
與監看設備狀態時，才可 configure：

```powershell
cmake -S . -B build/f407-can-m2006-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake `
  -DPNX_ENABLE_CAN_M2006_VALIDATION=ON
cmake --build build/f407-can-m2006-debug
```

## 架構與邊界

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

`testing.md` 是本機未追蹤測試計畫，不是 release evidence，也不應被自動
提交。正式公開發布前還必須由 repository owner 確認 root license/NOTICE
與 third-party distribution policy。
