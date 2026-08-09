#include "vehicle/combined/control/output_arbiter.hpp"

#include <cstdlib>

namespace
{

using vehicle::combined::control_mode;
using vehicle::combined::output_gate_input;
using vehicle::combined::select_outputs;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

void test_only_selected_manual_product_can_move() noexcept
{
    output_gate_input input{};
    input.mode = control_mode::chassis;
    input.chassis_ready = true;
    input.common_healthy = true;
    input.chassis_controller_enabled = true;
    input.arm_controller_enabled = true;

    auto output = select_outputs(input);
    require(output.chassis_enabled);
    require(!output.arm_manual_enabled);
    require(output.arm_hold_allowed);

    input.mode = control_mode::arm;
    output = select_outputs(input);
    require(!output.chassis_enabled);
    require(output.arm_manual_enabled);
    require(output.arm_hold_allowed);

    input.mode = control_mode::neutral;
    output = select_outputs(input);
    require(!output.chassis_enabled);
    require(!output.arm_manual_enabled);
    require(output.arm_hold_allowed);
}

void test_interlocks_and_faults_remove_outputs() noexcept
{
    output_gate_input input{};
    input.mode = control_mode::chassis;
    input.common_healthy = true;
    input.chassis_controller_enabled = true;
    input.arm_controller_enabled = true;

    auto output = select_outputs(input);
    require(!output.chassis_enabled);

    input.chassis_ready = true;
    input.common_healthy = false;
    output = select_outputs(input);
    require(!output.chassis_enabled);
    require(!output.arm_manual_enabled);
    require(!output.arm_hold_allowed);

    input.common_healthy = true;
    input.terminal_fault = true;
    output = select_outputs(input);
    require(!output.chassis_enabled);
    require(!output.arm_manual_enabled);
    require(!output.arm_hold_allowed);

    input.terminal_fault = false;
    input.chassis_controller_enabled = false;
    output = select_outputs(input);
    require(!output.chassis_enabled);
}

} // namespace

int main()
{
    test_only_selected_manual_product_can_move();
    test_interlocks_and_faults_remove_outputs();
    return EXIT_SUCCESS;
}
