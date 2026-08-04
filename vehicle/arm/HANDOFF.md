# 機械臂韌體 — clean extraction 交接

## 目前狀態

`rebuild/arm-clean` 從 Direct BSP authority `6b40de4` 重新提取，現在只包含能在
固定 submodule pins 上重現的 ARM 純邏輯基礎：三路舵機映射、J1 位置外環與
ARM configuration validation。分支沒有帶入 chassis、MaixCam、root
`CMakeLists.txt` 或任何 `pnx_*` gitlink 變更。

這不是完整 ARM product image：尚無 ARM selector、`app_start`、preset、
embedded source closure、CAN/PWM composition 或非零硬體輸出。

## 提取來源與範圍

- `feat/chassis@58f52f35c13081f059af7bfcb8a50eb92a07adf6`：
  `common/types.hpp`、`servo_map.*` 與 `arm_servo_map_tests.cpp`。
- ARM preservation checkpoint
  `fb573f7f115832e1f9760189a13e3293bd0eecc3`：`position_pid.*`、
  `arm_config.*` 及其 Host tests。
- `tests/host/CMakeLists.txt` 只提取上述三組 ARM Host test registration hunks。

## 刻意未納入的 runtime

Checkpoint 內的 `arm_runtime*`、`arm_safety_gate*` 與 runtime tests 暫不納入
clean branch。原因是它們把 J3 輸入綁到 `remoter::state::wheel`，但 authority
`pnx_modules@8ba925b60b11fec511a57622c199b57bb23f8f4e` 的公開 `state` 尚未提供
`wheel`。相關原始工作仍可由 `fb573f7` 重建；本分支沒有偷帶本地
`pnx_modules@bbb0a87` checkpoint，也沒有改變 gitlink。

恢復 runtime 前必須明確選擇並驗證其中一種契約：正式發布 shared remoter API，
或在 vehicle 層建立不改 shared pin 的 adapter／輸入映射。不得把目前的 PWM A2
BSP validation closure 當成 ARM product composition。

## 軟體驗證

- Host configure：`cmake -S . -B build/host-arm-clean -G Ninja
  -DPNX_HOST_TESTS=ON`，`PASS`。
- 聚焦測試：`arm_servo_map`、`arm_position_pid`、`arm_config`，3/3 `PASS`。
- Full Host CTest：47/47 `PASS`。
- Embedded ARM product build：`NOT_IMPLEMENTED`／`NOT_RUN`。
- 硬體上電、PWM 脈寬、CAN/M2006 與機械動作：`NOT_RUN`。
