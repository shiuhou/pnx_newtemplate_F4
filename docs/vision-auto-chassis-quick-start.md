# 视觉组快速开始

视觉端只负责两件事：识别目标，然后算出车体速度 `vx`、`vy`。把速度按 AVC1 格式从
MaixCam 发给 C 板即可。麦轮解算、四轮 PI、CAN、电机控制和安全停车都由 C 板处理。

完整协议细节和校验向量见
[AVC1 接口文档](vision-auto-chassis-interface.md)。第一次联调前请先看完本页；出现打包、
序号或校验问题时再查完整接口文档。

## 1. 接线

| MaixCam Pro | DJI C 板 | 说明 |
|---|---|---|
| TX | PG9／USART6_RX | 视觉命令由 MaixCam 发往 C 板 |
| GND | GND | 必须共地 |

使用 3.3 V TTL 串口，参数为 `115200, 8N1`。不要接 RS-232 电平，也不要假定 5 V
兼容。V1 是单向通信，C 板 PG14 TX 可以不接。

## 2. 速度方向

- `vx > 0`：小车向车头方向前进。
- `vx < 0`：小车后退。
- `vy > 0`：小车向车体左侧横移。
- `vy < 0`：小车向车体右侧横移。
- V1 不发送 `wz`，小车不会由视觉命令自转。

`vx`、`vy` 的单位都是 m/s，每个轴的范围是 `[-0.8, 0.8]`。第一次实车联调建议从
`0.1 m/s` 或更低开始，先确认方向，再逐步提高速度。

## 3. 发送频率与停车

以 20–50 Hz 持续发送，不要只在速度变化时发送。每发一帧，`seq` 都要加 1，包括
停车帧；达到 `0xFFFFFFFF` 后自然回到 0。

- `valid=1`：本帧的 `vx`、`vy` 可以参与自动控制。
- `valid=0`：主动停车，建议同时发送 `vx=0`、`vy=0`。
- 超过 200 ms 没有合法新帧：C 板自动停车。
- MaixCam 单独重启：`seq` 可以从 0 重新开始。旧命令超时后，C 板会接受新的序号
  基线。

## 4. 打包 24 字节帧

帧格式是小端序 `<IIffB3xI>`：

```text
magic | seq | vx | vy | valid | 3 字节零填充 | checksum
```

下面是最小 Python 打包函数，可直接移植到 MaixPy。它只负责生成一帧，不包含识别、
跟踪或串口循环。

```python
import struct

AVC1_MAGIC = 0x31435641


def avc1_checksum(data):
    value = 0xA5A51234
    for byte in data:
        value = ((value << 5) & 0xFFFFFFFF) ^ (value >> 2) ^ byte
    return value & 0xFFFFFFFF


def pack_avc1(seq, vx_mps, vy_mps, valid):
    vx_mps = max(-0.8, min(0.8, float(vx_mps)))
    vy_mps = max(-0.8, min(0.8, float(vy_mps)))
    payload = struct.pack(
        "<IIffB3x",
        AVC1_MAGIC,
        seq & 0xFFFFFFFF,
        vx_mps,
        vy_mps,
        1 if valid else 0,
    )
    return payload + struct.pack("<I", avc1_checksum(payload))
```

发送循环的核心形式：

```python
seq = 0

# 每次视觉循环：先得到 vx_mps、vy_mps 和 target_valid
frame = pack_avc1(seq, vx_mps, vy_mps, target_valid)
uart.write(frame)
seq = (seq + 1) & 0xFFFFFFFF
```

发送前可先核对：`len(pack_avc1(1, 0.5, -0.25, True))` 必须等于 24。完整接口文档中
还有三组已验证的字节级校验向量。

## 5. PS2 自动模式怎么开

1. 上电后默认是锁定的手动模式。
2. 按圆圈键解锁。
3. 按方块键进入自动模式。
4. C 板必须收到进入自动模式之后的新视觉帧。
5. 持续按住 L1，合法且新鲜的 `vx/vy` 才会驱动底盘。

松开 L1 会立即停车。三角键回到手动模式并停车；叉键会锁定并回到手动模式。自动
模式中 R1、R2 和手动机械臂控制均不生效。

## 6. 第一次联调按这个顺序

1. 先不按 L1，只发送 `valid=1` 的低速帧，确认底盘保持不动。
2. 发送 `valid=0`，确认底盘保持不动。
3. 经电控同学确认场地和硬件安全后，按住 L1，用不超过 `0.1 m/s` 的单轴命令检查
   前、后、左、右方向。
4. 松开 L1，确认立即停车。
5. 停止发送超过 200 ms，确认超时停车。
6. 重启 MaixCam，确认约 200 ms 安全停车后可以重新接收命令。

如果小车不动，先检查共地、串口参数、帧长度、校验和、`seq` 是否递增、是否已按
圆圈和方块，以及 L1 是否持续按住。不要先改 STM32、CAN、PI 或麦轮代码。

烧录和实车运动由电控组负责，需单独确认安全条件。视觉组不需要修改 C 板代码。
