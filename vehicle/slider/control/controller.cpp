#include "vehicle/slider/control/controller.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::slider
{
namespace
{

constexpr std::uint16_t bit(remoter::ps2_button button) noexcept
{
    return static_cast<std::uint16_t>(button);
}

bool trusted_ps2(const remoter::state& remote) noexcept
{
    return !remote.offline &&
           remote.active_source == remoter::source::ps2 &&
           remote.ps2_link == remoter::ps2_link_state::connected &&
           std::isfinite(remote.right_x) &&
           std::fabs(remote.right_x) <= 1.0F;
}

} // namespace

bool valid(const controller_configuration& config) noexcept
{
    return std::isfinite(config.manual_speed_rad_s) &&
           config.manual_speed_rad_s > 0.0F &&
           std::isfinite(config.automatic_speed_rad_s) &&
           config.automatic_speed_rad_s > 0.0F &&
           std::isfinite(config.deadband) && config.deadband >= 0.0F &&
           config.deadband < 1.0F &&
           std::isfinite(config.motor_direction) &&
           std::fabs(config.motor_direction) == 1.0F &&
           ::vehicle::chassis::valid(config.velocity_pi);
}

controller::controller(controller_configuration config) noexcept
    : config_(config), velocity_pi_(config.velocity_pi),
      config_valid_(valid(config))
{
}

controller_output controller::update(const controller_input& input) noexcept
{
    controller_output output{};
    output.mode = mode_;
    output.direction = direction_;

    if (!config_valid_ || !trusted_ps2(input.remote) ||
        !input.motor_online || !input.can_healthy ||
        !std::isfinite(input.measured_velocity_rad_s) ||
        !std::isfinite(input.dt_s) || input.dt_s <= 0.0F)
    {
        reset();
        return output;
    }

    const bool cross_pressed =
        (input.remote.ps2_pressed & bit(remoter::ps2_button::cross)) != 0U;
    const bool circle_pressed =
        (input.remote.ps2_pressed & bit(remoter::ps2_button::circle)) != 0U;
    if (cross_pressed)
    {
        mode_ = operating_mode::manual;
        manual_center_seen_ = false;
        velocity_pi_.reset();
    }
    else if (circle_pressed)
    {
        mode_ = operating_mode::automatic;
        velocity_pi_.reset();
    }

    output.mode = mode_;
    float target = 0.0F;
    if (mode_ == operating_mode::manual)
    {
        if (std::fabs(input.remote.right_x) <= config_.deadband)
        {
            manual_center_seen_ = true;
        }
        if (!manual_center_seen_)
        {
            velocity_pi_.reset();
            return output;
        }
        target = std::fabs(input.remote.right_x) <= config_.deadband
                     ? 0.0F
                     : input.remote.right_x * config_.manual_speed_rad_s;
        output.outputs_enabled = true;
    }
    else
    {
        if (!input.limits.ready ||
            (input.limits.left_active && input.limits.right_active))
        {
            velocity_pi_.reset();
            return output;
        }
        if (input.limits.right_active)
        {
            direction_ = automatic_direction::left;
        }
        else if (input.limits.left_active)
        {
            direction_ = automatic_direction::right;
        }
        target = direction_ == automatic_direction::right
                     ? config_.automatic_speed_rad_s
                     : -config_.automatic_speed_rad_s;
        output.outputs_enabled = true;
    }

    output.direction = direction_;
    output.target_velocity_rad_s = target;
    if (target == 0.0F)
    {
        velocity_pi_.reset();
        return output;
    }

    const float directed_measured =
        input.measured_velocity_rad_s * config_.motor_direction;
    const auto logical_current = velocity_pi_.update(
        target, directed_measured, input.dt_s);
    output.current_raw = static_cast<std::int16_t>(
        static_cast<float>(logical_current) * config_.motor_direction);
    return output;
}

void controller::reset() noexcept
{
    velocity_pi_.reset();
    mode_ = operating_mode::manual;
    direction_ = automatic_direction::right;
    manual_center_seen_ = false;
}

} // namespace vehicle::slider
