#include "vehicle/chassis/config.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::chassis::configuration;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

configuration valid_configuration() noexcept
{
    return {
        {0.076F, 0.18F, 0.16F, 100.0F},
        {0.05F, 1.0F, 1.0F, 2.0F, 1.0F, -1.0F, -1.0F},
        {1.0F, -1.0F, 1.0F, -1.0F},
        {1.0F, 0.0F, 0.0F, 1000.0F},
        0.005F,
    };
}

void default_configuration_is_exact_fail_closed_sentinel()
{
    const auto config = vehicle::chassis::mycar_configuration();

    require(config.geometry.wheel_radius_m == 0.0F);
    require(config.geometry.half_wheelbase_m == 0.0F);
    require(config.geometry.half_track_m == 0.0F);
    require(config.geometry.max_wheel_rad_s == 0.0F);
    require(config.manual.deadband == 0.05F);
    require(config.manual.max_vx_mps == 0.0F);
    require(config.manual.max_vy_mps == 0.0F);
    require(config.manual.max_yaw_rad_s == 0.0F);
    require(config.manual.vx_sign == 1.0F);
    require(config.manual.vy_sign == -1.0F);
    require(config.manual.yaw_sign == -1.0F);
    require((config.motor_direction ==
             std::array<float, 4U>{1.0F, 1.0F, 1.0F, 1.0F}));
    require(config.pi.kp == 0.0F);
    require(config.pi.ki_per_s == 0.0F);
    require(config.pi.integral_limit_raw == 0.0F);
    require(config.pi.current_limit_raw == 0.0F);
    require(config.control_period_s == 0.005F);
    require(!vehicle::chassis::valid(config));
}

void valid_configuration_requires_a_positive_finite_period()
{
    const auto config = valid_configuration();
    require(vehicle::chassis::valid(config));

    auto invalid = config;
    invalid.control_period_s = 0.0F;
    require(!vehicle::chassis::valid(invalid));

    invalid.control_period_s = -0.005F;
    require(!vehicle::chassis::valid(invalid));

    invalid.control_period_s =
        std::numeric_limits<float>::infinity();
    require(!vehicle::chassis::valid(invalid));

    invalid.control_period_s =
        std::numeric_limits<float>::quiet_NaN();
    require(!vehicle::chassis::valid(invalid));
}

} // namespace

int main()
{
    default_configuration_is_exact_fail_closed_sentinel();
    valid_configuration_requires_a_positive_finite_period();
    return 0;
}
