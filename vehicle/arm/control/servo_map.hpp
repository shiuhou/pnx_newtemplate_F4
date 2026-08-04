#pragma once

#include "vehicle/arm/common/types.hpp"

namespace vehicle::arm
{

// 检查 deadband 与每个舵机的安全脉宽窗口是否可计算。
bool valid(const servo_map_config& config) noexcept;

// 纯逻辑舵机映射：DR16 轴输入 -> 安全脉宽命令。
// 当前阶段只开放 J3 yaw 的增量控制；J2 与夹爪保持安全中位。
class servo_map {
public:
    explicit servo_map(servo_map_config config) noexcept;

    servo_map_output update(const servo_map_input& input,
                            float dt_s) noexcept;
    void reset() noexcept;

private:
    servo_map_output snapshot() const noexcept;

    servo_map_config config_{};
    std::array<float, servo_count> commanded_pulse_us_{};
    bool config_valid_{};
};

} // namespace vehicle::arm
