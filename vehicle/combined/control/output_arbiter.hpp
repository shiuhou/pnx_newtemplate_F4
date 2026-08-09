#pragma once

#include "vehicle/combined/control/mode_router.hpp"

namespace vehicle::combined
{

struct output_gate_input {
    control_mode mode{control_mode::neutral};
    bool chassis_ready{};
    bool common_healthy{};
    bool terminal_fault{};
    bool chassis_controller_enabled{};
    bool arm_controller_enabled{};
};

struct output_gate_output {
    bool chassis_enabled{};
    bool arm_manual_enabled{};
    bool arm_hold_allowed{};
};

output_gate_output select_outputs(const output_gate_input& input) noexcept;

} // namespace vehicle::combined
