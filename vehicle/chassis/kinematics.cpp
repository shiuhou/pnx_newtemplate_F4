#include "vehicle/chassis/kinematics.hpp"

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

bool valid(const body_velocity& command, const geometry& g) noexcept
{
    return finite(command.vx_mps) && finite(command.vy_mps) &&
           finite(command.yaw_rad_s) && finite(g.wheel_radius_m) &&
           finite(g.half_wheelbase_m) && finite(g.half_track_m) &&
           finite(g.max_wheel_rad_s) && g.wheel_radius_m > 0.0F &&
           g.half_wheelbase_m >= 0.0F && g.half_track_m >= 0.0F &&
           g.max_wheel_rad_s > 0.0F;
}

} // namespace

std::optional<wheel_vector> inverse_kinematics(
    const body_velocity& command, const geometry& g) noexcept
{
    if (!valid(command, g))
    {
        return std::nullopt;
    }

    const float k = g.half_wheelbase_m + g.half_track_m;
    wheel_vector out{{
        (command.vx_mps - command.vy_mps - k * command.yaw_rad_s) /
            g.wheel_radius_m,
        (command.vx_mps + command.vy_mps + k * command.yaw_rad_s) /
            g.wheel_radius_m,
        (command.vx_mps + command.vy_mps - k * command.yaw_rad_s) /
            g.wheel_radius_m,
        (command.vx_mps - command.vy_mps + k * command.yaw_rad_s) /
            g.wheel_radius_m,
    }};

    float maximum_magnitude = 0.0F;
    for (const float wheel_rad_s : out.rad_s)
    {
        if (!finite(wheel_rad_s))
        {
            return std::nullopt;
        }
        maximum_magnitude = std::max(
            maximum_magnitude, std::fabs(wheel_rad_s));
    }

    if (maximum_magnitude > g.max_wheel_rad_s)
    {
        const float scale = g.max_wheel_rad_s / maximum_magnitude;
        for (float& wheel_rad_s : out.rad_s)
        {
            wheel_rad_s *= scale;
        }
    }

    return out;
}

} // namespace vehicle::chassis
