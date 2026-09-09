#pragma once

#include "vehicle/slider/control/controller.hpp"

#include <bsp_can.hpp>

#include <cstdint>

namespace vehicle::slider
{

struct telemetry {
    operating_mode mode{operating_mode::manual};
    automatic_direction direction{automatic_direction::right};
    bool outputs_enabled{};
    bool watchdog_sampled{};
    bool motor_online{};
    bool limits_ready{};
    float right_x{};
    float target_velocity_rad_s{};
    float measured_velocity_rad_s{};
    std::int16_t current_raw{};
    std::uint32_t loop_count{};
    bsp::can::telemetry can{};
};

namespace runtime
{

void start(const controller_configuration& config) noexcept;
telemetry debug_state() noexcept;

} // namespace runtime
} // namespace vehicle::slider
