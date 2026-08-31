#include "vehicle/combined/control/mode_router.hpp"

#include <cmath>

namespace vehicle::combined
{
namespace
{

bool valid_deadband(float deadband) noexcept
{
    return std::isfinite(deadband) && deadband >= 0.0F && deadband < 1.0F;
}

bool centered(float axis, float deadband) noexcept
{
    return std::isfinite(axis) && std::fabs(axis) <= deadband;
}

bool trusted_remote(const remoter::state& remote) noexcept
{
    return !remote.offline &&
           (remote.active_source == remoter::source::dr16 ||
            (remote.active_source == remoter::source::ps2 &&
             remote.ps2_link == remoter::ps2_link_state::connected));
}

control_mode selected_mode(remoter::sw_state left_switch) noexcept
{
    switch (left_switch)
    {
    case remoter::sw_state::up:
        return control_mode::chassis;
    case remoter::sw_state::low:
        return control_mode::arm;
    case remoter::sw_state::mid:
    default:
        return control_mode::neutral;
    }
}

} // namespace

mode_router_output mode_router::update(const remoter::state& remote,
                                       float chassis_deadband,
                                       float arm_deadband) noexcept
{
    mode_router_output output{};
    output.chassis_remote = remote;

    const bool axes_finite = std::isfinite(remote.left_x) &&
                             std::isfinite(remote.left_y) &&
                             std::isfinite(remote.right_x) &&
                             std::isfinite(remote.right_y);
    const bool input_valid = valid_deadband(chassis_deadband) &&
                             valid_deadband(arm_deadband) && axes_finite;
    output.remote_online = input_valid && trusted_remote(remote);
    if (!output.remote_online)
    {
        chassis_ready_ = false;
        output.chassis_remote.offline = true;
        output.chassis_remote.active_source = remoter::source::none;
        output.chassis_remote.right_sw = remoter::sw_state::mid;
        output.chassis_remote.left_x = 0.0F;
        output.chassis_remote.left_y = 0.0F;
        output.chassis_remote.right_x = 0.0F;
        output.chassis_remote.right_y = 0.0F;
        return output;
    }

    output.mode = selected_mode(remote.left_sw);
    output.arm_axes_centered = centered(remote.right_y, arm_deadband) &&
                               centered(remote.left_y, arm_deadband) &&
                               centered(remote.right_x, arm_deadband) &&
                               centered(remote.left_x, arm_deadband);

    const bool chassis_selected = output.mode == control_mode::chassis;
    const bool ps2_source =
        remote.active_source == remoter::source::ps2;
    const bool chassis_axes_centered =
        centered(remote.left_x, chassis_deadband) &&
        centered(remote.right_x, chassis_deadband) &&
        centered(ps2_source ? remote.right_y : remote.left_y,
                 chassis_deadband);
    if (!chassis_selected || remote.right_sw != remoter::sw_state::up)
    {
        chassis_ready_ = false;
    }
    else if (!chassis_ready_ && chassis_axes_centered)
    {
        chassis_ready_ = true;
    }

    output.chassis_ready = chassis_selected && chassis_ready_;
    if (!output.chassis_ready)
    {
        output.chassis_remote.right_sw = remoter::sw_state::mid;
        output.chassis_remote.left_x = 0.0F;
        output.chassis_remote.left_y = 0.0F;
        output.chassis_remote.right_x = 0.0F;
        output.chassis_remote.right_y = 0.0F;
    }
    return output;
}

void mode_router::reset() noexcept
{
    chassis_ready_ = false;
}

} // namespace vehicle::combined
