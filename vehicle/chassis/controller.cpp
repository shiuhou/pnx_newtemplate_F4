#include "vehicle/chassis/controller.hpp"

#include <cmath>

namespace vehicle::chassis
{
namespace
{

bool valid_geometry(const geometry& value) noexcept
{
    return std::isfinite(value.wheel_radius_m) &&
           value.wheel_radius_m > 0.0F &&
           std::isfinite(value.half_wheelbase_m) &&
           value.half_wheelbase_m >= 0.0F &&
           std::isfinite(value.half_track_m) &&
           value.half_track_m >= 0.0F &&
           std::isfinite(value.max_wheel_rad_s) &&
           value.max_wheel_rad_s > 0.0F;
}

bool valid_manual_limits(const manual_limits& value) noexcept
{
    return std::isfinite(value.deadband) && value.deadband >= 0.0F &&
           value.deadband < 1.0F && std::isfinite(value.max_vx_mps) &&
           value.max_vx_mps > 0.0F && std::isfinite(value.max_vy_mps) &&
           value.max_vy_mps > 0.0F &&
           std::isfinite(value.max_yaw_rad_s) &&
           value.max_yaw_rad_s > 0.0F && std::isfinite(value.vx_sign) &&
           std::fabs(value.vx_sign) == 1.0F &&
           std::isfinite(value.vy_sign) &&
           std::fabs(value.vy_sign) == 1.0F &&
           std::isfinite(value.yaw_sign) &&
           std::fabs(value.yaw_sign) == 1.0F;
}

bool valid_manual_sample(const manual_input& value) noexcept
{
    return std::isfinite(value.left_x) &&
           std::isfinite(value.left_y) &&
           std::isfinite(value.right_x);
}

} // namespace

bool valid(const controller_configuration& config) noexcept
{
    if (!valid_geometry(config.geometry) ||
        !valid_manual_limits(config.manual) || !valid(config.pi))
    {
        return false;
    }

    for (const float direction : config.motor_direction)
    {
        if (!std::isfinite(direction) || std::fabs(direction) != 1.0F)
        {
            return false;
        }
    }
    return true;
}

controller::controller(controller_configuration config) noexcept
    : config_(config),
      config_valid_(valid(config)),
      pi_{velocity_pi{config.pi}, velocity_pi{config.pi},
          velocity_pi{config.pi}, velocity_pi{config.pi}}
{
}

controller_output controller::update(
    const manual_input& manual,
    const wheel_vector& measured_motor_rad_s,
    const safety_input& safety,
    float dt_s) noexcept
{
    controller_output output{};
    safety_input gated_safety = safety;
    gated_safety.remote_online = safety.remote_online && manual.online;
    gated_safety.arm_switches_up =
        safety.arm_switches_up && manual.arm_switches_up;
    gated_safety.config_valid = safety.config_valid && config_valid_ &&
                                valid_manual_sample(manual);
    output.state = safety_.update(gated_safety);

    bool target_valid = false;
    if (config_valid_)
    {
        const auto target = inverse_kinematics(
            map_manual(manual, config_.manual), config_.geometry);
        if (target.has_value())
        {
            output.wheel_target_rad_s = *target;
            target_valid = true;
        }
    }

    if (!safety_.output_enabled() || !target_valid ||
        !std::isfinite(dt_s) || dt_s <= 0.0F)
    {
        reset_pi();
        return output;
    }

    for (const float measured : measured_motor_rad_s.rad_s)
    {
        if (!std::isfinite(measured))
        {
            reset_pi();
            return output;
        }
    }

    std::array<float, 4U> motor_target_rad_s{};
    for (std::size_t index = 0U; index < pi_.size(); ++index)
    {
        motor_target_rad_s[index] =
            output.wheel_target_rad_s.rad_s[index] *
            config_.motor_direction[index];
        const float error =
            motor_target_rad_s[index] - measured_motor_rad_s.rad_s[index];
        if (!std::isfinite(motor_target_rad_s[index]) ||
            !std::isfinite(error))
        {
            reset_pi();
            return output;
        }
    }

    for (std::size_t index = 0U; index < pi_.size(); ++index)
    {
        output.motor_current_raw[index] = pi_[index].update(
            motor_target_rad_s[index], measured_motor_rad_s.rad_s[index], dt_s);
    }
    return output;
}

void controller::reset() noexcept
{
    safety_.reset();
    reset_pi();
}

void controller::reset_pi() noexcept
{
    for (velocity_pi& pi : pi_)
    {
        pi.reset();
    }
}

} // namespace vehicle::chassis
