#include "vehicle/chassis/control/command_slew.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::chassis
{
namespace
{

bool finite(float value) noexcept
{
    return std::isfinite(value);
}

bool finite(const body_velocity& value) noexcept
{
    return finite(value.vx_mps) && finite(value.vy_mps) &&
           finite(value.yaw_rad_s);
}

float move_toward(float current, float target, float maximum_delta) noexcept
{
    if (target > current)
    {
        return std::min(current + maximum_delta, target);
    }
    return std::max(current - maximum_delta, target);
}

float step_axis(float current, float target, float accel_step,
                float decel_step) noexcept
{
    // 反向時先只減到零；下一個週期才使用 accel rate 往反方向走。
    const bool reversing = current != 0.0F && target != 0.0F &&
                           std::signbit(current) != std::signbit(target);
    if (reversing)
    {
        return move_toward(current, 0.0F, decel_step);
    }

    const bool reducing_magnitude = std::fabs(target) < std::fabs(current);
    return move_toward(current, target,
                       reducing_magnitude ? decel_step : accel_step);
}

} // namespace

bool valid(const command_slew_config& config) noexcept
{
    return finite(config.translation_accel_mps2) &&
           config.translation_accel_mps2 > 0.0F &&
           finite(config.translation_decel_mps2) &&
           config.translation_decel_mps2 > 0.0F &&
           finite(config.yaw_accel_rad_s2) &&
           config.yaw_accel_rad_s2 > 0.0F &&
           finite(config.yaw_decel_rad_s2) &&
           config.yaw_decel_rad_s2 > 0.0F;
}

command_slew_limiter::command_slew_limiter(
    command_slew_config config) noexcept
    : config_(config), config_valid_(valid(config))
{
}

body_velocity command_slew_limiter::update(
    const body_velocity& target, float dt_s) noexcept
{
    if (!config_valid_ || !finite(target) || !finite(dt_s) || dt_s <= 0.0F)
    {
        reset();
        return {};
    }

    const float translation_accel_step =
        config_.translation_accel_mps2 * dt_s;
    const float translation_decel_step =
        config_.translation_decel_mps2 * dt_s;
    const float yaw_accel_step = config_.yaw_accel_rad_s2 * dt_s;
    const float yaw_decel_step = config_.yaw_decel_rad_s2 * dt_s;
    if (!finite(translation_accel_step) ||
        !finite(translation_decel_step) || !finite(yaw_accel_step) ||
        !finite(yaw_decel_step))
    {
        reset();
        return {};
    }

    current_.vx_mps = step_axis(
        current_.vx_mps, target.vx_mps,
        translation_accel_step, translation_decel_step);
    current_.vy_mps = step_axis(
        current_.vy_mps, target.vy_mps,
        translation_accel_step, translation_decel_step);
    current_.yaw_rad_s = step_axis(
        current_.yaw_rad_s, target.yaw_rad_s,
        yaw_accel_step, yaw_decel_step);
    return current_;
}

void command_slew_limiter::reset() noexcept
{
    current_ = {};
}

} // namespace vehicle::chassis
