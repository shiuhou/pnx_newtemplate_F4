#pragma once

#include "vehicle/arm/common/types.hpp"
#include "vehicle/arm/control/j1_stall_guard.hpp"
#include "vehicle/arm/control/position_pid.hpp"
#include "vehicle/chassis/control/velocity_pi.hpp"

namespace vehicle::arm
{

// Parameters for the attended DR16-controlled J1-J4 arm closure.
struct configuration {
    position_pid_config j1_position;
    ::vehicle::chassis::velocity_pi_config j1_velocity;
    float j1_manual_position_rate_rad_per_s{};
    float j1_position_min_rad{};
    float j1_position_max_rad{};
    float j1_manual_deadband{};
    j1_stall_guard_config j1_stall{};
    float j1_gravity_amplitude_raw{};
    float j1_gravity_phase_rad{};
    float j1_gravity_bias_raw{};
    float j1_motor_direction{};
    servo_map_config servos{};
    float control_period_s{};
};

configuration arm_configuration() noexcept;
bool valid(const configuration& config) noexcept;

} // namespace vehicle::arm
