#include "vehicle/chassis/manual_control.hpp"

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

bool valid_limits(const manual_limits& limits) noexcept
{
    return finite(limits.deadband) && limits.deadband >= 0.0F &&
           limits.deadband < 1.0F && finite(limits.max_vx_mps) &&
           limits.max_vx_mps > 0.0F && finite(limits.max_vy_mps) &&
           limits.max_vy_mps > 0.0F && finite(limits.max_yaw_rad_s) &&
           limits.max_yaw_rad_s > 0.0F && finite(limits.vx_sign) &&
           finite(limits.vy_sign) && finite(limits.yaw_sign) &&
           std::fabs(limits.vx_sign) == 1.0F &&
           std::fabs(limits.vy_sign) == 1.0F &&
           std::fabs(limits.yaw_sign) == 1.0F;
}

float apply_deadband(float axis, float deadband) noexcept
{
    const float bounded = std::clamp(axis, -1.0F, 1.0F);
    const float magnitude = std::fabs(bounded);
    if (magnitude <= deadband)
    {
        return 0.0F;
    }

    const float scaled = (magnitude - deadband) / (1.0F - deadband);
    return std::copysign(scaled, bounded);
}

} // namespace

body_velocity map_manual(const manual_input& input,
                         const manual_limits& limits) noexcept
{
    if (!input.online || !input.arm_switches_up || !finite(input.left_x) ||
        !finite(input.left_y) || !finite(input.right_x) ||
        !valid_limits(limits))
    {
        return {};
    }

    return {
        apply_deadband(input.left_y, limits.deadband) *
            limits.max_vx_mps * limits.vx_sign,
        apply_deadband(input.left_x, limits.deadband) *
            limits.max_vy_mps * limits.vy_sign,
        apply_deadband(input.right_x, limits.deadband) *
            limits.max_yaw_rad_s * limits.yaw_sign,
    };
}

} // namespace vehicle::chassis
