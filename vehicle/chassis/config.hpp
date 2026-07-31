#pragma once

#include "vehicle/chassis/controller.hpp"

#include <array>

namespace vehicle::chassis
{

struct configuration {
    ::vehicle::chassis::geometry geometry;
    manual_limits manual;
    std::array<float, 4U> motor_direction;
    velocity_pi_config pi;
    float control_period_s;
};

configuration mycar_configuration() noexcept;
bool valid(const configuration& config) noexcept;

} // namespace vehicle::chassis
