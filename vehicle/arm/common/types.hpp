#pragma once

#include "bsp_pwm.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace vehicle::arm
{

inline constexpr std::size_t servo_count = 3U;

// 机械臂当前三个 PWM 执行器的逻辑顺序：J2、J3、夹爪。
// J1 为 M2006/CAN 闭环，不走这里的 PWM 映射。
enum class servo_axis : std::uint8_t {
    j2_pitch = 0U,
    j3_yaw = 1U,
    gripper = 2U,
};

struct servo_map_axis_config {
    bsp::pwm::channel channel{bsp::pwm::none};
    float center_pulse_us{};
    float min_pulse_us{};
    float max_pulse_us{};
    float max_rate_us_per_s{};
};

struct servo_map_config {
    float deadband{};
    std::array<servo_map_axis_config, servo_count> axes{};
};

struct servo_map_input {
    bool online{};
    float j3_yaw_axis{};
};

struct servo_map_output {
    std::array<std::uint32_t, servo_count> pulse_us{};
};

inline constexpr std::size_t servo_index(servo_axis axis) noexcept
{
    return static_cast<std::size_t>(axis);
}

} // namespace vehicle::arm
