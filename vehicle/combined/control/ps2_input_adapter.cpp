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
    entered_auto_ = false;
    stop_requested_ = false;
    l1_held_ = false;
    if (remote.active_source == remoter::source::dr16)
    {
        reset();
        return remote;
    }

    if (!valid_ps2(remote))
    {
        reset();
        stop_requested_ = true;
        return fail_closed(remote);
    }

    const bool circle_pressed =
        (remote.ps2_pressed & bit(remoter::ps2_button::circle)) != 0U;
    const bool cross_pressed =
        (remote.ps2_pressed & bit(remoter::ps2_button::cross)) != 0U;
    const bool square_pressed =
        (remote.ps2_pressed & bit(remoter::ps2_button::square)) != 0U;
    const bool triangle_pressed =
        (remote.ps2_pressed & bit(remoter::ps2_button::triangle)) != 0U;
    const bool circle_held =
        remoter::is_held(remote.ps2_buttons, remoter::ps2_button::circle);
    if (!circle_held)
    {
        circle_release_seen_ = true;
    }

    const bool previously_unlocked = unlocked_;
    // Lock wins if multiple mode edges arrive together. A fresh release is required
    // before Circle can unlock after startup or a PS2 reconnect.
    if (cross_pressed)
    {
        unlocked_ = false;
        mode_ = operator_mode::manual;
        stop_requested_ = true;
    }
    else
    {
        if (circle_pressed && circle_release_seen_)
        {
            unlocked_ = true;
        }
        if (triangle_pressed)
        {
            mode_ = operator_mode::manual;
            stop_requested_ = true;
        }
        else if (square_pressed && previously_unlocked && unlocked_)
        {
            entered_auto_ = mode_ != operator_mode::vision_auto;
            mode_ = operator_mode::vision_auto;
        }
    }

    l1_held_ =
        remoter::is_held(remote.ps2_buttons, remoter::ps2_button::l1);
    const bool r1_held =
        remoter::is_held(remote.ps2_buttons, remoter::ps2_button::r1);
    const bool r2_held =
        remoter::is_held(remote.ps2_buttons, remoter::ps2_button::r2);

    remoter::state output = remote;
    output.left_sw = remoter::sw_state::mid;
    if (mode_ == operator_mode::manual && !stop_requested_ &&
        unlocked_ && r1_held != r2_held)
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
    l1_held_ = false;
    entered_auto_ = false;
    stop_requested_ = false;
    mode_ = operator_mode::manual;
}

operator_mode ps2_input_adapter::mode() const noexcept
{
    return mode_;
}

bool ps2_input_adapter::unlocked() const noexcept
{
    return unlocked_;
}

bool ps2_input_adapter::l1_held() const noexcept
{
    return l1_held_;
}

bool ps2_input_adapter::entered_auto() const noexcept
{
    return entered_auto_;
}

bool ps2_input_adapter::stop_requested() const noexcept
{
    return stop_requested_;
}

} // namespace vehicle::combined
