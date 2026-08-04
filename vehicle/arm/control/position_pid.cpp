#include "vehicle/arm/control/position_pid.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::arm
{

bool valid(const position_pid_config& config) noexcept
{
    return std::isfinite(config.kp) && config.kp >= 0.0F &&
           std::isfinite(config.kd) && config.kd >= 0.0F &&
           (config.kp > 0.0F || config.kd > 0.0F) &&
           std::isfinite(config.target_velocity_limit_rad_s) &&
           config.target_velocity_limit_rad_s > 0.0F;
}

position_pid::position_pid(position_pid_config config) noexcept
    : config_(config), config_valid_(valid(config))
{
}

float position_pid::update(float target_position_rad,
                           float measured_position_rad,
                           float measured_velocity_rad_s,
                           float dt_s) noexcept
{
    // 與 velocity_pi 保持同一契約：非法設定、NaN/Inf 或非正 dt 都直接 fail-closed。
    if (!config_valid_ || !std::isfinite(target_position_rad) ||
        !std::isfinite(measured_position_rad) ||
        !std::isfinite(measured_velocity_rad_s) || !std::isfinite(dt_s) ||
        dt_s <= 0.0F)
    {
        reset();
        return 0.0F;
    }

    // 正誤差代表目標比目前位置更高，應命令正向角速度，也就是 J1 往上抬起。
    const float error_rad = target_position_rad - measured_position_rad;
    if (!std::isfinite(error_rad))
    {
        reset();
        return 0.0F;
    }

    const float unsaturated_rad_s =
        config_.kp * error_rad - config_.kd * measured_velocity_rad_s;
    const float saturated_rad_s = std::clamp(
        unsaturated_rad_s, -config_.target_velocity_limit_rad_s,
        config_.target_velocity_limit_rad_s);
    if (!std::isfinite(saturated_rad_s))
    {
        reset();
        return 0.0F;
    }

    last_output_rad_s_ = saturated_rad_s;
    return saturated_rad_s;
}

void position_pid::reset() noexcept
{
    last_output_rad_s_ = 0.0F;
}

} // namespace vehicle::arm
