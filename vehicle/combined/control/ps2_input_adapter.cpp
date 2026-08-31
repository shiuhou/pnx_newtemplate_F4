#include "vehicle/combined/control/ps2_input_adapter.hpp"

#include <cmath>
#include <cstdint>

namespace vehicle::combined
{
namespace
{

constexpr std::uint16_t bit(remoter::ps2_button button) noexcept
{
    return static_cast<std::uint16_t>(button);
}

bool valid_axis(float axis) noexcept
{
    return std::isfinite(axis) && std::fabs(axis) <= 1.0F;
}

bool valid_ps2(const remoter::state& remote) noexcept
{
    return !remote.offline &&
           remote.active_source == remoter::source::ps2 &&
           remote.ps2_link == remoter::ps2_link_state::connected &&
           valid_axis(remote.left_x) && valid_axis(remote.left_y) &&
           valid_axis(remote.right_x) && valid_axis(remote.right_y);
}

remoter::state fail_closed(const remoter::state& remote) noexcept
{
    remoter::state output = remote;
    output.offline = true;
    output.left_sw = remoter::sw_state::mid;
    output.right_sw = remoter::sw_state::mid;
    output.left_x = 0.0F;
    output.left_y = 0.0F;
    output.right_x = 0.0F;
    output.right_y = 0.0F;
    output.wheel = 0.0F;
    return output;
}

} // namespace

remoter::state ps2_input_adapter::update(
    const remoter::state& remote) noexcept
{
    if (remote.active_source == remoter::source::dr16)
    {
        reset();
        return remote;
    }

    if (!valid_ps2(remote))
    {
        reset();
        return fail_closed(remote);
    }

    const bool circle_pressed =
        (remote.ps2_pressed & bit(remoter::ps2_button::circle)) != 0U;
    const bool cross_pressed =
        (remote.ps2_pressed & bit(remoter::ps2_button::cross)) != 0U;
    const bool circle_held =
        remoter::is_held(remote.ps2_buttons, remoter::ps2_button::circle);
    if (!circle_held)
    {
        circle_release_seen_ = true;
    }

    // Lock wins if both edges arrive together. A fresh release is required
    // before Circle can unlock after startup or a PS2 reconnect.
    if (cross_pressed)
    {
        unlocked_ = false;
    }
    else if (circle_pressed && circle_release_seen_)
    {
        unlocked_ = true;
    }

    const bool r1_held =
        remoter::is_held(remote.ps2_buttons, remoter::ps2_button::r1);
    const bool r2_held =
        remoter::is_held(remote.ps2_buttons, remoter::ps2_button::r2);

    remoter::state output = remote;
    output.left_sw = remoter::sw_state::mid;
    if (unlocked_ && r1_held != r2_held)
    {
        output.left_sw = r1_held ? remoter::sw_state::up
                                 : remoter::sw_state::low;
    }
    output.right_sw = unlocked_ ? remoter::sw_state::up
                                : remoter::sw_state::mid;
    return output;
}

void ps2_input_adapter::reset() noexcept
{
    unlocked_ = false;
    circle_release_seen_ = false;
}

} // namespace vehicle::combined
