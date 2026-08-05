#pragma once

#include <cstdint>

namespace vehicle::arm
{

enum class j1_stall_direction : std::int8_t {
    negative = -1,
    none = 0,
    positive = 1,
};

struct j1_stall_guard_config {
    float current_threshold_raw{};
    float velocity_threshold_rad_s{};
    float position_error_threshold_rad{};
    float timeout_s{};
};

bool valid(const j1_stall_guard_config& config) noexcept;

struct j1_stall_guard_input {
    bool enabled{};
    float manual_axis{};
    float target_position_rad{};
    float measured_position_rad{};
    float measured_velocity_rad_s{};
    std::int16_t commanded_current_raw{};
    float dt_s{};
};

struct j1_stall_guard_output {
    float allowed_manual_axis{};
    float stall_elapsed_s{};
    j1_stall_direction blocked_direction{j1_stall_direction::none};
    bool hold_target{};
};

class j1_stall_guard {
public:
    explicit j1_stall_guard(j1_stall_guard_config config) noexcept;

    j1_stall_guard_output update(
        const j1_stall_guard_input& input) noexcept;
    void reset() noexcept;

private:
    j1_stall_guard_config config_{};
    float stall_elapsed_s_{};
    j1_stall_direction blocked_direction_{j1_stall_direction::none};
    bool config_valid_{};
};

} // namespace vehicle::arm
