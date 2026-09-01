# PS2 自动视觉底盘 UART 接口（AVC1）

本文是 MaixCam 视觉组与 STM32F407 电控组之间的 V1 联调契约。视觉组完成识别、跟随控制与速度规划，只向 C 板发送最终车体平移速度；C 板负责安全仲裁、加速度限制、麦轮逆解、四轮 PI 与 M2006/CAN 输出。

## 接线与串口

| MaixCam Pro | DJI C 板 F407 | 说明 |
|---|---|---|
| TX | PG9 / USART6_RX | V1 命令单向传输 |
| GND | GND | 必须共地 |
| RX | PG14 / USART6_TX | V1 不使用，不提供 ACK |

两端按 **3.3 V TTL UART** 连接并必须共地；不要接 RS-232 电平，也不要假定
5 V 兼容。V1 只需要 MaixCam TX、C 板 PG9 RX 和 GND，PG14 TX 可以不接。
串口固定为 `115200, 8N1`。USART6 只由 PS2 combined 产品使用；不要将视觉
串口与手柄或裁判系统串口复用。

## 24-byte 固定帧

所有多字节字段均为 little-endian；浮点数为 IEEE-754 binary32。Python `struct` 格式为 `<IIffB3xI`。

| Offset | Size | Field | 语义 |
|---:|---:|---|---|
| 0 | 4 | `magic` | 固定 `0x31435641`，线上字节为 `41 56 43 31`（`AVC1`） |
| 4 | 4 | `seq` | `uint32_t` 传输序号 |
| 8 | 4 | `vx_mps` | 向车头为正，单位 m/s |
| 12 | 4 | `vy_mps` | 向车体左侧为正，单位 m/s |
| 16 | 1 | `valid` | 只允许 `0` 或 `1`；`0` 表示主动停车 |
| 17 | 3 | padding | 必须全为 `0` |
| 20 | 4 | `checksum` | 对 bytes `[0, 20)` 计算 |

V1 不包含 `wz`，C 板始终使用 `wz=0`。`vx`、`vy` 分别限幅到 `[-0.8, 0.8] m/s`；这是逐轴限幅，不是平移向量模长限制。最终轮速仍经过现有麦轮 normalization。

checksum 初值为 `0xA5A51234`，依次处理前 20 个字节：

```cpp
c = ((c << 5U) & 0xFFFFFFFFU) ^ (c >> 2U) ^ byte;
```

## Golden vectors

以下三组包含完整 24-byte 帧，可直接用于核对打包、字节序、padding 与 checksum。
它们是独立测试向量，不表示应按 `1 -> 2 -> 0` 的顺序连续发送；第三组用于
核对 `0xFFFFFFFF -> 0` wrap 后的 `seq=0` frame。

```text
seq=1, vx=0.5, vy=-0.25, valid=1
41 56 43 31 01 00 00 00 00 00 00 3F 00 00 80 BE
01 00 00 00 2E B8 88 88
checksum=0x8888B82E

seq=2, vx=0, vy=0, valid=0
41 56 43 31 02 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 DE B9 EB 44
checksum=0x44EBB9DE

seq=0, vx=-0.8, vy=0.8, valid=1
41 56 43 31 00 00 00 00 CD CC 4C BF CD CC 4C 3F
01 00 00 00 ED 6D 71 00
checksum=0x00716DED
```

## Sequence 与错误处理

C 板只接受比上一帧新的 sequence：

```cpp
static bool sequence_is_newer(std::uint32_t current,
                              std::uint32_t previous) noexcept
{
    return static_cast<std::int32_t>(current - previous) > 0;
}
```

因此 `0xFFFFFFFF -> 0` 是合法 wrap。发送端不得一次跳过超过 `2^31` 个 sequence。重复、倒序、magic 错误、padding 非零、checksum 错误、`valid` 非 0/1、NaN/Inf 均被丢弃，而且不会刷新 200 ms timeout。

视觉端建议以 **20–50 Hz** 持续发送。每个 frame 都必须递增 `seq`，包括
`valid=0` 的主动停车 frame；不要只在速度变化时才发送。

接收流允许一帧被拆开、多帧粘连或前面带垃圾字节。接收 queue overflow 会立即作废当前命令；下一帧合法新命令到达后可以恢复。`valid=0` 或连续 200 ms 没有合法新帧都会停车。

MaixCam 单独重启时可以从 `seq=0` 重新计数。在旧命令超过 200 ms 未刷新后，
C 板会让下一帧 checksum/field 合法的 frame 建立新的 sequence baseline；
200 ms 活跃窗口内的重复或倒序 frame 仍会被拒绝。

## PS2 操作

| 操作 | 结果 |
|---|---|
| 上电、PS2 断线/重连、Cross | 锁定并回到 MANUAL |
| Circle | 全局解锁 |
| 已解锁后按 Square | 进入 AUTO；必须等待进入后的一帧新视觉命令 |
| Triangle | 回到 MANUAL，并立即停车 |
| MANUAL + R1 | 原手动底盘控制 |
| MANUAL + R2 | 原手动机械臂控制 |
| AUTO + 按住 L1 | 只有视觉帧合法、新鲜且底盘健康时才执行 `vx/vy` |
| AUTO + 松开 L1 | 当控制周期立即停车 |

AUTO V1 独占底盘，忽略 R1/R2，并禁用手动机械臂。CAN、电机或配置健康失败继续沿用既有 fail-closed 行为。

## Build 与联调顺序

在本 repository 根目录执行：

```powershell
cmake --fresh --preset f407-mycar-combined-ps2-debug
cmake --build --preset f407-mycar-combined-ps2-debug
```

联调时依次确认：

1. 不连接视觉串口，上电、解锁、进入 AUTO 后底盘保持零输出。
2. 发送 `valid=0` golden vector，底盘保持零输出。
3. 按住 L1 后发送低速合法命令，确认前后/左右方向；V1 全程不得产生旋转命令。
4. 松开 L1、发送 `valid=0`、停止发送超过 200 ms、拔掉 UART，分别确认当周期或 timeout 内停车。
5. 短暂坏帧或垃圾字节后发送新的合法 frame，确认 parser 能重新同步。
6. Triangle、Cross 与 PS2 断线分别确认回到安全状态；重新进入 AUTO 时旧视觉帧不得驱动车辆。

烧录与实车动作需要另外明确授权；软件 build 通过不等于硬件验收通过。
