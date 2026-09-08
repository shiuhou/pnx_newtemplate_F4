#include "vehicle/combined/control/output_arbiter.hpp"

namespace vehicle::combined
{

output_gate_output select_outputs(const output_gate_input& input) noexcept
{
    output_gate_output output{};
    const bool chassis_health =
        input.chassis_healthy && !input.terminal_fault;
    const bool arm_health = input.arm_healthy && !input.terminal_fault;
    const bool chassis_mode_ready =
        (input.mode == control_mode::chassis && input.chassis_ready) ||
        (input.mode == control_mode::vision_auto && input.vision_ready);
    output.chassis_enabled =
        chassis_health && chassis_mode_ready &&
        input.chassis_controller_enabled;
    output.arm_manual_enabled =
        arm_health && input.mode == control_mode::arm &&
        input.arm_controller_enabled;
    output.arm_hold_allowed = arm_health;
    return output;
}

} // namespace vehicle::combined
