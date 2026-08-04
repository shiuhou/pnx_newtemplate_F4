#include "vehicle/chassis/control/command_slew.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using namespace vehicle::chassis;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool near(float actual, float expected) noexcept
{
    return std::isfinite(actual) &&
           std::fabs(actual - expected) < 0.0001F;
}

void require_velocity(const body_velocity& actual,
                      const body_velocity& expected) noexcept
{
    require(near(actual.vx_mps, expected.vx_mps));
    require(near(actual.vy_mps, expected.vy_mps));
    require(near(actual.yaw_rad_s, expected.yaw_rad_s));
}

void acceleration_is_bounded_per_axis() noexcept
{
    constexpr command_slew_config limits{4.5F, 6.0F, 6.0F, 9.0F};
    command_slew_limiter limiter{limits};

    require_velocity(
        limiter.update({1.5F, -1.5F, 1.8F}, 0.005F),
        {0.0225F, -0.0225F, 0.03F});
}

void normal_stop_uses_deceleration_rates() noexcept
{
    constexpr command_slew_config limits{4.5F, 6.0F, 6.0F, 9.0F};
    command_slew_limiter limiter{limits};
    require_velocity(limiter.update({1.5F, -1.5F, 1.8F}, 1.0F),
                     {1.5F, -1.5F, 1.8F});

    require_velocity(limiter.update({}, 0.1F),
                     {0.9F, -0.9F, 0.9F});
    require_velocity(limiter.update({}, 1.0F), {});
}

void reversal_stops_at_zero_before_accelerating_opposite() noexcept
{
    constexpr command_slew_config limits{1.0F, 2.0F, 3.0F, 4.0F};
    command_slew_limiter limiter{limits};
    require_velocity(limiter.update({1.0F, 0.0F, 1.0F}, 1.0F),
                     {1.0F, 0.0F, 1.0F});

    require_velocity(limiter.update({-1.0F, 0.0F, -1.0F}, 0.25F),
                     {0.5F, 0.0F, 0.0F});
    require_velocity(limiter.update({-1.0F, 0.0F, -1.0F}, 0.25F),
                     {0.0F, 0.0F, -0.75F});
    require_velocity(limiter.update({-1.0F, 0.0F, -1.0F}, 0.25F),
                     {-0.25F, 0.0F, -1.0F});
}

void invalid_input_resets_state() noexcept
{
    constexpr command_slew_config limits{4.5F, 6.0F, 6.0F, 9.0F};
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();

    for (const float invalid_dt : {0.0F, -0.005F, nan, infinity})
    {
        command_slew_limiter limiter{limits};
        require_velocity(limiter.update({1.5F, 0.0F, 0.0F}, 0.1F),
                         {0.45F, 0.0F, 0.0F});
        require_velocity(limiter.update({1.5F, 0.0F, 0.0F}, invalid_dt), {});
        require_velocity(limiter.update({1.5F, 0.0F, 0.0F}, 0.005F),
                         {0.0225F, 0.0F, 0.0F});
    }

    command_slew_limiter invalid_target{limits};
    require_velocity(invalid_target.update({1.5F, 0.0F, 0.0F}, 0.1F),
                     {0.45F, 0.0F, 0.0F});
    require_velocity(invalid_target.update({nan, 0.0F, 0.0F}, 0.005F), {});
    require_velocity(invalid_target.update({1.5F, 0.0F, 0.0F}, 0.005F),
                     {0.0225F, 0.0F, 0.0F});

    invalid_target.reset();
    require_velocity(invalid_target.update({0.0F, -1.5F, 0.0F}, 0.005F),
                     {0.0F, -0.0225F, 0.0F});
}

void invalid_configuration_fails_closed() noexcept
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    for (const command_slew_config invalid : {
             command_slew_config{0.0F, 6.0F, 6.0F, 9.0F},
             command_slew_config{4.5F, -1.0F, 6.0F, 9.0F},
             command_slew_config{4.5F, 6.0F, nan, 9.0F},
             command_slew_config{4.5F, 6.0F, 6.0F, 0.0F},
         })
    {
        require(!valid(invalid));
        command_slew_limiter limiter{invalid};
        require_velocity(limiter.update({1.0F, 1.0F, 1.0F}, 0.1F), {});
    }
}

} // namespace

int main()
{
    acceleration_is_bounded_per_axis();
    normal_stop_uses_deceleration_rates();
    reversal_stops_at_zero_before_accelerating_opposite();
    invalid_input_resets_state();
    invalid_configuration_fails_closed();
    return EXIT_SUCCESS;
}
