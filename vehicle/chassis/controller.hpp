#pragma once

#include "vehicle/chassis/kinematics.hpp"
#include "vehicle/chassis/manual_control.hpp"
#include "vehicle/chassis/safety_gate.hpp"
#include "vehicle/chassis/velocity_pi.hpp"

#include <array>
#include <cstdint>

namespace vehicle::chassis
{

struct controller_configuration {
    ::vehicle::chassis::geometry geometry;
    manual_limits manual;
    std::array<float, 4U> motor_direction;
    velocity_pi_config pi;
};

bool valid(const controller_configuration& config) noexcept;

struct controller_output {
    wheel_vector wheel_target_rad_s;
    std::array<std::int16_t, 4U> motor_current_raw;
    safety_state state;
};

class controller {
public:
    explicit controller(controller_configuration config) noexcept;

    controller_output update(
        const manual_input& manual,
        const wheel_vector& measured_motor_rad_s,
        const safety_input& safety,
        float dt_s) noexcept;
    void reset() noexcept;

private:
    void reset_pi() noexcept;

    controller_configuration config_;
    bool config_valid_{};
    safety_gate safety_{};
    std::array<velocity_pi, 4U> pi_;
};

} // namespace vehicle::chassis
