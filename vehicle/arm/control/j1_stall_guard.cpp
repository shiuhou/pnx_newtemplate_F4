#include "vehicle/arm/control/j1_stall_guard.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::arm
{
namespace
{

j1_stall_direction direction_for(float axis) noexcept
{
    if (axis > 0.0F)
    {
        return j1_stall_direction::positive;
    }
    if (axis < 0.0F)
    {
        return j1_stall_direction::negative;
    }
    return j1_stall_direction::none;
}

bool opposite(j1_stall_direction lhs,
              j1_stall_direction rhs) noexcept
{
    return (lhs == j1_stall_direction::positive &&
            rhs == j1_stall_direction::negative) ||
           (lhs == j1_stall_direction::negative &&
            rhs == j1_stall_direction::positive);
}

} // namespace

bool valid(const j1_stall_guard_config& config) noexcept
{
    return std::isfinite(config.current_threshold_raw) &&
           config.current_threshold_raw > 0.0F &&
           std::isfinite(config.velocity_threshold_rad_s) &&
           config.velocity_threshold_rad_s > 0.0F &&
           std::isfinite(config.position_error_threshold_rad) &&
           config.position_error_threshold_rad > 0.0F &&
           std::isfinite(config.timeout_s) && config.timeout_s > 0.0F;
}

j1_stall_guard::j1_stall_guard(j1_stall_guard_config config) noexcept
    : config_(config), config_valid_(valid(config))
{
}

j1_stall_guard_output j1_stall_guard::update(
    const j1_stall_guard_input& input) noexcept
{
    j1_stall_guard_output output{};
    if (!input.enabled)
    {
        reset();
        return output;
    }

    if (!config_valid_ || !std::isfinite(input.manual_axis) ||
        !std::isfinite(input.target_position_rad) ||
        !std::isfinite(input.measured_position_rad) ||
        !std::isfinite(input.measured_velocity_rad_s) ||
        !std::isfinite(input.dt_s) || input.dt_s <= 0.0F)
    {
        reset();
        return output;
    }

    const float axis = std::clamp(input.manual_axis, -1.0F, 1.0F);
    const auto requested_direction = direction_for(axis);
    if (blocked_direction_ != j1_stall_direction::none)
    {
        if (opposite(requested_direction, blocked_direction_))
        {
            blocked_direction_ = j1_stall_direction::none;
            stall_elapsed_s_ = 0.0F;
            output.allowed_manual_axis = axis;
        }
        output.blocked_direction = blocked_direction_;
        output.stall_elapsed_s = stall_elapsed_s_;
        return output;
    }

    output.allowed_manual_axis = axis;
    if (requested_direction == j1_stall_direction::none)
    {
        stall_elapsed_s_ = 0.0F;
        return output;
    }

    const float position_error_rad =
        input.target_position_rad - input.measured_position_rad;
    const bool loaded_in_requested_direction =
        (requested_direction == j1_stall_direction::positive &&
         position_error_rad >= config_.position_error_threshold_rad) ||
        (requested_direction == j1_stall_direction::negative &&
         position_error_rad <= -config_.position_error_threshold_rad);
    const bool high_current =
        std::fabs(static_cast<float>(input.commanded_current_raw)) >=
        config_.current_threshold_raw;
    const bool nearly_stationary =
        std::fabs(input.measured_velocity_rad_s) <=
        config_.velocity_threshold_rad_s;

    if (!loaded_in_requested_direction || !high_current ||
        !nearly_stationary)
    {
        stall_elapsed_s_ = 0.0F;
        return output;
    }

    const float next_elapsed_s = stall_elapsed_s_ + input.dt_s;
    if (next_elapsed_s >= config_.timeout_s - input.dt_s * 0.5F)
    {
        stall_elapsed_s_ = config_.timeout_s;
        blocked_direction_ = requested_direction;
        output.allowed_manual_axis = 0.0F;
        output.hold_target = true;
    }
    else
    {
        stall_elapsed_s_ = next_elapsed_s;
    }
    output.stall_elapsed_s = stall_elapsed_s_;
    output.blocked_direction = blocked_direction_;
    return output;
}

void j1_stall_guard::reset() noexcept
{
    stall_elapsed_s_ = 0.0F;
    blocked_direction_ = j1_stall_direction::none;
}

} // namespace vehicle::arm
