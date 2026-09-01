# MyCar F407：PS2 手动与视觉自动底盘／机械臂

本仓库是 DJI C 板（STM32F407）车辆的 `mycar/f4` 专属组合。当前竞赛产品有两个
互斥的命令来源：

```text
PS2 手动：R1 底盘／R2 机械臂 -----------------------------┐
MaixCam 自动：USART6 AVC1 vx/vy + L1 使能 ----------------┤
                                                          v
body_velocity -> 速度斜坡 -> X 型麦轮 -> 四轮速度 PI
              -> 限幅电流 -> CAN1 上的四个 M2006
```

这是车辆产品仓库，不是通用底盘框架。车辆代码位于 `vehicle/`；固定版本的
`pnx_*` 子模块及其公开 API 仍由上游维护。

## 当前状态

PS2 手动基线已发布在 `chassis_x_arm`。本分支增加完整的 AVC1 视觉命令链路与
手动／自动仲裁。Host 测试和所有受影响的 F407 构建均通过；新的自动模式 ELF
尚未烧录或进行硬件操作。

2026-08-04，操作者已验证硬件接受的 `DA37D784...` ELF：完成三次冷启动／重新
解锁、DR16 掉线后零输出恢复、五分钟地面行驶无爬行或意外掉线，并确认前后、横移
和自转方向正确。后续归属清理保留共享遥控器轴语义，并将本车左摇杆轴交换放在
`vehicle/chassis`；其 `92691430...` ELF 已由 OpenOCD 烧录、校验、复位，操作者
确认可重新解锁、前后、横移、自转与回中停车。本次仅为该精确产物的短时硬件通过；
五分钟行驶和 DR16 掉线的完整证据仍对应 `DA37D784...`。

```text
软件门槛：通过
已接受的硬件基线 ELF：操作者观察通过
当前归属清理 ELF：烧录校验通过，短时硬件通过
PS2 视觉自动软件：通过，硬件未运行
```

## 当前控制方式

- 上电、PS2 掉线／重连和叉键均为“锁定 + 手动”；圆圈键解锁。
- 手动模式中，R1 保留既有底盘行为，R2 保留既有机械臂行为。
- 只有解锁后按方块键才进入自动模式；自动模式忽略 R1/R2，且必须持续按住 L1。
- 进入自动模式后，必须收到一帧新的 AVC1 命令才允许运动。`valid=0`、UART 静默
  超过 200 ms、队列溢出、松开 L1、三角键、叉键或既有 CAN／电机／配置故障，都会
  在当前周期置零并清除底盘速度斜坡和 PI 状态。
- MaixCam 负责识别和跟随控制计算；C 板只接收最终 `vx/vy`，自动模式的 `wz` 恒为
  零。详细约定见 [AVC1 接口文档](docs/vision-auto-chassis-interface.md)。

## 阅读路线

本分支的最短新功能阅读路线是：

1. [`docs/vision-auto-chassis-interface.md`](docs/vision-auto-chassis-interface.md)
   了解视觉组唯一需要遵守的 UART 契约。
2. `vehicle/combined/control/vision_command.*`：帧、字节队列、超时与自动使能条件。
3. `vehicle/combined/control/ps2_input_adapter.*`：圆圈／方块／三角／叉／L1 的语义。
4. `vehicle/combined/runtime/runtime.cpp` 看命令仲裁如何进入同一个
   `vehicle/chassis/control/controller.*`。

不要由 STM32 HAL、CMSIS、ThreadX、USBX 或 `pnx_*` 子模块反向猜测车辆设计；它们分别
是第三方或上游实现。建议按下列顺序阅读目前 MyCar 路径：

1. [`AGENTS.md`](AGENTS.md)：先确定车辆代码归属、失效归零与授权边界。
2. [`CMakeLists.txt`](CMakeLists.txt) 与 `CMakePresets.json`：确认当前产品实际编译
   哪些源文件。
3. [`configs/vehicles/mycar_combined/params.ps2.json`](configs/vehicles/mycar_combined/params.ps2.json)
   與 `robot.json`：前者选择 PS2／UART 绑定，后者绑定四个 M2006 的 CAN ID；JSON 不
   支持注释，语义由生成 CMake 和本表说明。
4. [`demo/app.cpp`](demo/app.cpp) → [`vehicle/combined.cpp`](vehicle/combined.cpp)：
   从共同入口进入 combined runtime 的最短调用链。
5. `vehicle/chassis/common/` → `control/` → `runtime/`：按数据类型、纯控制、ThreadX／
   CAN／遥控器整合的方向阅读。这是车辆功能的权威实现。
6. [`boards/dji_c_board_f407/README.md`](boards/dji_c_board_f407/README.md)，再配合
   `pnx_bsp/can/src/bsp_can.cpp`、`pnx_bsp/usart/src/bsp_usart.cpp`：只在需要
   追到 F407 HAL、生成的句柄或引脚时再阅读。
7. `tests/host/chassis_*` 与 `tests/host/combined_*`：将前述安全、运动学与控制规则
   作为可执行规格阅读。

`demo/cboard/*`、USB、PWM、SPI、BMI088 与其他示例都是独立闭包或历史验证；它们可
用于理解 F407 基线，但不是本车 PS2／M2006／视觉链路。CubeMX 生成文件与第三方目录
同理，只在追查底层问题时阅读，不直接修改车辆功能。

获取已发布的视觉自动分支：

```powershell
git clone --branch feat/ps2-auto-vision --recurse-submodules `
  https://github.com/shiuhou/pnx_newtemplate_F4.git pnx_f4_auto_vision
cd pnx_f4_auto_vision
```

需要 CMake 3.22+、Ninja、用于 Host 测试的本机 C++ 编译器，以及用于 F407 构建的
GNU Arm Embedded Toolchain。

运行 Host 测试：

```powershell
cmake -S . -B build/host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

构建 PS2 手动 + 视觉自动的组合映像：

```powershell
cmake --preset f407-mycar-combined-ps2-debug
cmake --build --preset f407-mycar-combined-ps2-debug
```

产物是 `build/f407-mycar-combined-ps2-debug/pnx_embedded.elf`。构建成功仅代表软件
证据；未经单独授权，不得烧录或连接非零电机输出。

## 安全与授权边界


- MyCar 配置记录实测几何尺寸、现场测试的速度／电流限制、方向与 PI 参数。非法或
  非有限值仍会失效归零。
- 底盘控制器要求在线遥控器先观察到解除使能状态、再观察到使能上升沿，才能解锁；
  掉线状态的默认开关值不能满足该启动互锁。
- 遥控器掉线、电机／CAN 健康失败、配置非法、手动输入格式错误和控制周期超时，都会
  选择松弛／零输出，并在适用时重置 PI 状态。
- 视觉无效／超时／溢出和松开 L1 属于可恢复停车，不会新增永久故障锁定；只有所有
  自动条件再次成立时，后续合法帧才能恢复。
- 硬件操作、烧录、非零电流、commit、push 与跨工作区写入都需要用户明确授权。

当前增益和限制是已验证的可行驶基线，不是最终比赛整定。提高输出前必须重新测量和
验证。

## 源码与配置索引

| 需求 | 权威位置 |
| --- | --- |
| 车辆控制核心与运行层适配 | `vehicle/chassis/common/`、`control/`、`runtime/` |
| 车辆组合入口 | `vehicle/combined.cpp` |
| 组合 PS2／视觉运行层 | `vehicle/combined/runtime/runtime.cpp` |
| AVC1 解析器、队列、快照与使能条件 | `vehicle/combined/control/vision_command.*` |
| PS2 手动／自动输入 | `vehicle/combined/control/ps2_input_adapter.*` |
| 视觉组 UART 契约 | `docs/vision-auto-chassis-interface.md` |
| 几何、限制、方向与 PI／电流参数 | `vehicle/chassis/runtime/config.cpp` |
| PS2／视觉 UART 与四个 M2006 身份 | `configs/vehicles/mycar_combined/params.ps2.json`、`robot.json` |
| MyCar 构建选择 | `CMakeLists.txt`、`CMakePresets.json` |
| Host 行为与回归测试 | `tests/host/chassis_*`、`tests/host/combined_*` |
| F407 Direct BSP 实现 | `pnx_bsp/*/src/` |
| CubeMX／生成句柄、启动、链接与 RTOS 整合 | `boards/dji_c_board_f407/` |

当前数据流：

```text
USART1 PS2 ─┐
            ├-> 命令仲裁 -> vehicle/chassis 运行层
USART6 AVC1 ─┘                -> 轮速目标／电流命令 -> DJI 电机处理器 -> CAN1
M2006 反馈 0x201..0x204 ----------------------------------------------------^
```

车轮顺序为 FL/FR/RL/RR。坐标为 `+x` 向前、`+y` 向左，正 yaw 为逆时针。CAN ID、
电机方向和实车前后／横移／自转响应已于 2026-08-04 验证。

## 人员与 AI 代理工作指引

修改前先阅读 [AGENTS.md](AGENTS.md)，其中定义仓库归属、失效归零规则、授权边界及
默认 `STANDARD` 工程模式。

之后只阅读能回答当前问题的最小文档集：

1. 本 README：范围、命令与安全边界。
2. `vehicle/chassis/`、`vehicle/combined/` 与 `configs/vehicles/mycar_combined/`：
   实现或参数问题。
3. [HANDOFF.md](HANDOFF.md)：证据历史和当前交接状态。
4. `.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/` 下的任务记录：仅在需要详细
   验证来源时阅读。

不要扫描、修改或从历史原型推断当前设计。车辆专属改动不得修改共享 `pnx_*` 子模块或
公开 API。

## 后续需授权的工作

任务 7 是断电状态下测量轮半径、轴距、轮距、轮子到 CAN ID 映射、正方向、安全
速度／电流限制和初始 PI 参数。

任务 8 只能在明确授权后开始：烧录、零电流台架检查、实时 PS2／CAN 观察、单轮方向
检查、抬车四轮低速测试、地面测试、重复运行与长时间稳定性测试。

精确的验收门槛与证据限制见
[实施计划](docs/superpowers/plans/2026-07-31-f4-mycar-dr16-m2006-chassis.md) 和
[验证报告](.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/validation-report.md)。

## 共享 F407 基线

本分支使用 F407 Board／BSP 基础，不重新定义该基础。Board 归属、CubeMX 规则与外设
实现见 [C 板 README](boards/dji_c_board_f407/README.md)。共享公开契约仍位于固定版本的
`pnx_bsp`、`pnx_devices`、`pnx_libs` 和 `pnx_modules` 子模块中。
