#include "vehicle/combined/control/output_arbiter.hpp"

namespace vehicle::combined
{

output_gate_output select_outputs(const output_gate_input& input) noexcept
{
    output_gate_output output{};
    const bool output_health =
        input.common_healthy && !input.terminal_fault;
    output.chassis_enabled =
        output_health && input.mode == control_mode::chassis &&
        input.chassis_ready && input.chassis_controller_enabled;
    output.arm_manual_enabled =
        output_health && input.mode == control_mode::arm &&
        input.arm_controller_enabled;
    output.arm_hold_allowed = output_health;
    return output;
}

} // namespace vehicle::combined
