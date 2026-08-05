#pragma once

#include "vehicle/arm/control/position_pid.hpp"

namespace vehicle::arm
{

// safety gate 已允許 J1 手動閉環後，這個純邏輯 helper 會把 right_y
// 轉成目標位置積分，再交給位置外環算出目標角速度。
struct j1_manual_command_input {
    bool enabled{};
    float manual_axis{}; // right_y，-1..+1；正值代表 J1 往上抬起。
    float measured_position_rad{};
    float measured_velocity_rad_s{};
    float dt_s{};
};

struct j1_manual_command_output {
    float target_position_rad{};
    float target_velocity_rad_s{};
};

class j1_manual_command {
public:
    j1_manual_command(position_pid_config outer_loop,
                      float manual_position_rate_rad_per_s,
                      float position_min_rad,
                      float position_max_rad,
                      float deadband) noexcept;

    j1_manual_command_output update(
        const j1_manual_command_input& input) noexcept;
    void reset() noexcept;

private:
    position_pid outer_loop_{{}};
    j1_manual_command_output output_{};
    float manual_position_rate_rad_per_s_{};
    float position_min_rad_{};
    float position_max_rad_{};
    float deadband_{};
    bool config_valid_{};
    bool target_position_seeded_{};
};

} // namespace vehicle::arm
