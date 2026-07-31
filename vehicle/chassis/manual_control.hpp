#pragma once

#include "vehicle/chassis/types.hpp"

namespace vehicle::chassis
{

struct manual_input {
    bool online{};
    bool arm_switches_up{};
    float left_x{};
    float left_y{};
    float right_x{};
};

struct manual_limits {
    float deadband{};
    float max_vx_mps{};
    float max_vy_mps{};
    float max_yaw_rad_s{};
    float vx_sign{};
    float vy_sign{};
    float yaw_sign{};
};

body_velocity map_manual(const manual_input& input,
                         const manual_limits& limits) noexcept;

} // namespace vehicle::chassis
