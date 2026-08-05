#include "vehicle/arm/runtime/arm_config.hpp"

#include "vehicle/arm/control/servo_map.hpp"

#include "pwm_channels.hpp"

#include <cmath>

namespace vehicle::arm
{
namespace
{

constexpr float pi = 3.14159265358979323846F;

} // namespace

configuration arm_configuration() noexcept
{
    return {
        // Position error -> target output-shaft velocity.
        {3.0F, 0.2F, 1.2F},
        // Velocity error -> C610 raw current, capped for attended bring-up.
        {7000.0F, 0.0F, 0.0F, 4000.0F},
        1.2F,
        0.0F,
        pi,
        0.08F,
        {3500.0F, 0.05F, 0.15F, 2.0F},
        500.0F,
        0.0F,
        0.0F,
        -1.0F,
        {
            0.08F,
            {{
                // The seed is used only after deliberate input; unlock sends no pulse.
                {board::pwm::servo_c2,300.0F, 300.0F, 2800.0F, 1000.0F},
                {board::pwm::servo_pe13, 1500.0F, 900.0F, 2100.0F, 900.0F},
                {board::pwm::servo_pe14, 1500.0F, 1000.0F, 2000.0F, 500.0F},
            }},
        },
        0.005F,
    };
}

bool valid(const configuration& config) noexcept
{
    return valid(config.j1_position) &&
           ::vehicle::chassis::valid(config.j1_velocity) &&
           std::isfinite(config.j1_manual_position_rate_rad_per_s) &&
           config.j1_manual_position_rate_rad_per_s > 0.0F &&
           std::isfinite(config.j1_position_min_rad) &&
           std::isfinite(config.j1_position_max_rad) &&
           config.j1_position_min_rad < config.j1_position_max_rad &&
           std::isfinite(config.j1_manual_deadband) &&
           config.j1_manual_deadband >= 0.0F &&
           config.j1_manual_deadband < 1.0F &&
           valid(config.j1_stall) &&
           config.j1_stall.current_threshold_raw <=
               config.j1_velocity.current_limit_raw &&
           std::isfinite(config.j1_gravity_amplitude_raw) &&
           std::fabs(config.j1_gravity_amplitude_raw) <=
               config.j1_velocity.current_limit_raw &&
           std::isfinite(config.j1_gravity_phase_rad) &&
           std::isfinite(config.j1_gravity_bias_raw) &&
           std::fabs(config.j1_gravity_bias_raw) <=
               config.j1_velocity.current_limit_raw &&
           std::isfinite(config.j1_motor_direction) &&
           std::fabs(config.j1_motor_direction) == 1.0F &&
           valid(config.servos) &&
           std::isfinite(config.control_period_s) &&
           config.control_period_s > 0.0F;
}

} // namespace vehicle::arm
