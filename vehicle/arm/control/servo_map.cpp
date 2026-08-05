#include "vehicle/arm/control/servo_map.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::arm
{
namespace
{

bool finite(float value) noexcept
{
    return std::isfinite(value);
}

bool valid_axis(const servo_map_axis_config& axis) noexcept
{
    return axis.channel != bsp::pwm::none && finite(axis.center_pulse_us) &&
           finite(axis.min_pulse_us) && finite(axis.max_pulse_us) &&
           finite(axis.max_rate_us_per_s) && axis.min_pulse_us > 0.0F &&
           axis.min_pulse_us <= axis.center_pulse_us &&
           axis.center_pulse_us <= axis.max_pulse_us &&
           axis.max_pulse_us < 65536.0F && axis.max_rate_us_per_s > 0.0F;
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

std::uint32_t round_to_pulse(float value) noexcept
{
    return static_cast<std::uint32_t>(std::lround(value));
}

float pulse_for_first_axis(const servo_map_axis_config& config,
                           float axis) noexcept
{
    const float span = axis >= 0.0F
                           ? config.max_pulse_us - config.center_pulse_us
                           : config.center_pulse_us - config.min_pulse_us;
    return config.center_pulse_us + axis * span;
}

} // namespace

bool valid(const servo_map_config& config) noexcept
{
    if (!finite(config.deadband) || config.deadband < 0.0F ||
        config.deadband >= 1.0F)
    {
        return false;
    }

    for (const servo_map_axis_config& axis : config.axes)
    {
        if (!valid_axis(axis))
        {
            return false;
        }
    }
    return true;
}

servo_map::servo_map(servo_map_config config) noexcept
    : config_(config), config_valid_(valid(config))
{
    reset();
}

servo_map_output servo_map::update(const servo_map_input& input,
                                   float dt_s) noexcept
{
    if (!config_valid_ || !input.online || !finite(input.j2_pitch_axis) ||
        !finite(input.j3_yaw_axis) || !finite(input.gripper_axis) ||
        !finite(dt_s) || dt_s <= 0.0F)
    {
        reset();
        return snapshot();
    }

    const std::size_t j2 = servo_index(servo_axis::j2_pitch);
    const servo_map_axis_config& pitch = config_.axes[j2];
    const float pitch_axis =
        apply_deadband(input.j2_pitch_axis, config_.deadband);
    if (pitch_axis != 0.0F)
    {
        if (!command_active_[j2])
        {
            commanded_pulse_us_[j2] =
                pulse_for_first_axis(pitch, pitch_axis);
        }
        else
        {
            const float pitch_delta =
                pitch_axis * pitch.max_rate_us_per_s * dt_s;
            commanded_pulse_us_[j2] = std::clamp(
                commanded_pulse_us_[j2] + pitch_delta,
                pitch.min_pulse_us, pitch.max_pulse_us);
        }
        command_active_[j2] = true;
    }

    const std::size_t j3 = servo_index(servo_axis::j3_yaw);
    const servo_map_axis_config& yaw = config_.axes[j3];
    const float yaw_axis =
        apply_deadband(input.j3_yaw_axis, config_.deadband);
    if (yaw_axis != 0.0F)
    {
        if (!command_active_[j3])
        {
            commanded_pulse_us_[j3] =
                pulse_for_first_axis(yaw, yaw_axis);
        }
        else
        {
            const float yaw_delta =
                yaw_axis * yaw.max_rate_us_per_s * dt_s;
            commanded_pulse_us_[j3] = std::clamp(
                commanded_pulse_us_[j3] + yaw_delta,
                yaw.min_pulse_us, yaw.max_pulse_us);
        }
        command_active_[j3] = true;
    }

    const std::size_t gripper = servo_index(servo_axis::gripper);
    const servo_map_axis_config& gripper_config = config_.axes[gripper];
    const float gripper_axis =
        apply_deadband(input.gripper_axis, config_.deadband);
    if (gripper_axis != 0.0F)
    {
        if (!command_active_[gripper])
        {
            commanded_pulse_us_[gripper] =
                pulse_for_first_axis(gripper_config, gripper_axis);
        }
        else
        {
            const float gripper_delta =
                gripper_axis * gripper_config.max_rate_us_per_s * dt_s;
            commanded_pulse_us_[gripper] = std::clamp(
                commanded_pulse_us_[gripper] + gripper_delta,
                gripper_config.min_pulse_us,
                gripper_config.max_pulse_us);
        }
        command_active_[gripper] = true;
    }
    return snapshot();
}

void servo_map::reset() noexcept
{
    for (std::size_t index = 0U; index < servo_count; ++index)
    {
        commanded_pulse_us_[index] = config_.axes[index].center_pulse_us;
        command_active_[index] = false;
    }
}

servo_map_output servo_map::snapshot() const noexcept
{
    servo_map_output output{};
    for (std::size_t index = 0U; index < servo_count; ++index)
    {
        if (command_active_[index])
        {
            output.pulse_us[index] =
                round_to_pulse(commanded_pulse_us_[index]);
        }
    }
    return output;
}

} // namespace vehicle::arm
