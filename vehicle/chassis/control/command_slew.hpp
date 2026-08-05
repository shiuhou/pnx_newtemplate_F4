#pragma once

#include "vehicle/chassis/common/types.hpp"

namespace vehicle::chassis
{

// 車體速度指令的分軸加減速限制；平移與 yaw 使用各自單位。
struct command_slew_config {
    float translation_accel_mps2{};
    float translation_decel_mps2{};
    float yaw_accel_rad_s2{};
    float yaw_decel_rad_s2{};
};

// 四個 rate 必須都是有限正數；非法設定由 controller fail-closed。
bool valid(const command_slew_config& config) noexcept;

// 有狀態的車體速度斜坡。安全事件由 controller 呼叫 reset() 立即清零；
// 正常搖桿回中則把零目標交給 update()，按 decel rate 平滑停车。
class command_slew_limiter {
public:
    explicit command_slew_limiter(command_slew_config config) noexcept;

    body_velocity update(const body_velocity& target, float dt_s) noexcept;
    void reset() noexcept;

private:
    command_slew_config config_{};
    body_velocity current_{};
    bool config_valid_{};
};

} // namespace vehicle::chassis
