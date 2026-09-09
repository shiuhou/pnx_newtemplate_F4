#pragma once

#include "pnx_modules/remoter/include/types.hpp"
#include "vehicle/chassis/control/velocity_pi.hpp"

#include <cstdint>

namespace vehicle::slider
{

enum class operating_mode : std::uint8_t {
    manual,
    automatic,
};

enum class automatic_direction : std::uint8_t {
    left,
    right,
};

struct limit_state {
    bool ready{};
    bool left_active{};
    bool right_active{};
};

struct controller_configuration {
    float manual_speed_rad_s{};
    float automatic_speed_rad_s{};
    float deadband{};
    float motor_direction{};
    ::vehicle::chassis::velocity_pi_config velocity_pi{};
};

struct controller_input {
    remoter::state remote{};
    limit_state limits{};
    float measured_velocity_rad_s{};
    bool motor_online{};
    bool can_healthy{};
    float dt_s{};
};

struct controller_output {
    operating_mode mode{operating_mode::manual};
    automatic_direction direction{automatic_direction::right};
    bool outputs_enabled{};
    float target_velocity_rad_s{};
    std::int16_t current_raw{};
};

bool valid(const controller_configuration& config) noexcept;

class controller {
public:
    explicit controller(controller_configuration config) noexcept;

    controller_output update(const controller_input& input) noexcept;
    void reset() noexcept;

private:
    controller_configuration config_{};
    ::vehicle::chassis::velocity_pi velocity_pi_{{}};
    operating_mode mode_{operating_mode::manual};
    automatic_direction direction_{automatic_direction::right};
    bool manual_center_seen_{};
    bool config_valid_{};
};

} // namespace vehicle::slider
