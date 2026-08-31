#include "vehicle/combined/control/ps2_input_adapter.hpp"

#include <cstdlib>
#include <limits>

namespace
{

using vehicle::combined::ps2_input_adapter;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

constexpr std::uint16_t bit(remoter::ps2_button button) noexcept
{
    return static_cast<std::uint16_t>(button);
}

remoter::state connected_ps2() noexcept
{
    remoter::state remote{};
    remote.offline = false;
    remote.active_source = remoter::source::ps2;
    remote.ps2_link = remoter::ps2_link_state::connected;
    return remote;
}

void press(remoter::state& remote, remoter::ps2_button button) noexcept
{
    remote.ps2_buttons |= bit(button);
    remote.ps2_pressed = bit(button);
}

void test_circle_unlocks_and_cross_locks() noexcept
{
    ps2_input_adapter adapter{};
    auto remote = connected_ps2();

    auto output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::mid);

    press(remote, remoter::ps2_button::circle);
    output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::up);

    remote.ps2_buttons = 0U;
    remote.ps2_pressed = 0U;
    output = adapter.update(remote);
    require(output.right_sw == remoter::sw_state::up);

    press(remote, remoter::ps2_button::cross);
    output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::mid);
}

void test_unlock_requires_fresh_circle_press_after_startup() noexcept
{
    ps2_input_adapter adapter{};
    auto remote = connected_ps2();
    remote.ps2_buttons = bit(remoter::ps2_button::circle);
    remote.ps2_pressed = bit(remoter::ps2_button::circle);

    auto output = adapter.update(remote);
    require(output.right_sw == remoter::sw_state::mid);

    remote.ps2_buttons = 0U;
    remote.ps2_pressed = 0U;
    output = adapter.update(remote);
    require(output.right_sw == remoter::sw_state::mid);

    press(remote, remoter::ps2_button::circle);
    output = adapter.update(remote);
    require(output.right_sw == remoter::sw_state::up);
}

void test_r1_selects_chassis_and_r2_selects_arm_while_unlocked() noexcept
{
    ps2_input_adapter adapter{};
    auto remote = connected_ps2();
    (void)adapter.update(remote);
    press(remote, remoter::ps2_button::circle);
    (void)adapter.update(remote);

    remote.ps2_pressed = 0U;
    remote.ps2_buttons = bit(remoter::ps2_button::r1);
    auto output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::up);
    require(output.right_sw == remoter::sw_state::up);

    remote.ps2_buttons = bit(remoter::ps2_button::r2);
    output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::low);
    require(output.right_sw == remoter::sw_state::up);

    remote.ps2_buttons = bit(remoter::ps2_button::r1) |
                         bit(remoter::ps2_button::r2);
    output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::up);

    remote.ps2_buttons = 0U;
    output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::up);
}

void test_locked_shoulders_never_select_a_mode() noexcept
{
    ps2_input_adapter adapter{};
    auto remote = connected_ps2();
    remote.ps2_buttons = bit(remoter::ps2_button::r1);
    auto output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::mid);

    remote.ps2_buttons = bit(remoter::ps2_button::r2);
    output = adapter.update(remote);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::mid);
}

void test_link_loss_and_reconnect_clear_unlock() noexcept
{
    ps2_input_adapter adapter{};
    auto remote = connected_ps2();
    (void)adapter.update(remote);
    press(remote, remoter::ps2_button::circle);
    require(adapter.update(remote).right_sw == remoter::sw_state::up);

    remote.ps2_link = remoter::ps2_link_state::remote_disconnected;
    remote.left_x = 0.5F;
    auto output = adapter.update(remote);
    require(output.offline);
    require(output.left_x == 0.0F);
    require(output.right_sw == remoter::sw_state::mid);

    remote = connected_ps2();
    remote.ps2_buttons = bit(remoter::ps2_button::r1);
    output = adapter.update(remote);
    require(!output.offline);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::mid);
}

void test_conflicting_lock_edges_fail_closed_to_locked() noexcept
{
    ps2_input_adapter adapter{};
    auto remote = connected_ps2();
    (void)adapter.update(remote);
    remote.ps2_pressed = bit(remoter::ps2_button::circle) |
                         bit(remoter::ps2_button::cross);
    remote.ps2_buttons = remote.ps2_pressed |
                         bit(remoter::ps2_button::r1);
    const auto output = adapter.update(remote);
    require(!output.offline);
    require(output.left_sw == remoter::sw_state::mid);
    require(output.right_sw == remoter::sw_state::mid);
}

void test_invalid_axes_fail_closed() noexcept
{
    ps2_input_adapter adapter{};
    auto remote = connected_ps2();
    remote.right_y = std::numeric_limits<float>::quiet_NaN();
    auto output = adapter.update(remote);
    require(output.offline);
    require(output.right_y == 0.0F);

    remote = connected_ps2();
    remote.left_x = 1.01F;
    output = adapter.update(remote);
    require(output.offline);
    require(output.left_x == 0.0F);
}

void test_dr16_passes_through_and_resets_ps2_unlock() noexcept
{
    ps2_input_adapter adapter{};
    auto ps2 = connected_ps2();
    (void)adapter.update(ps2);
    press(ps2, remoter::ps2_button::circle);
    require(adapter.update(ps2).right_sw == remoter::sw_state::up);

    remoter::state dr16{};
    dr16.offline = false;
    dr16.active_source = remoter::source::dr16;
    dr16.left_sw = remoter::sw_state::low;
    dr16.right_sw = remoter::sw_state::up;
    dr16.left_x = 0.25F;
    const auto passed = adapter.update(dr16);
    require(!passed.offline);
    require(passed.active_source == remoter::source::dr16);
    require(passed.left_sw == remoter::sw_state::low);
    require(passed.right_sw == remoter::sw_state::up);
    require(passed.left_x == 0.25F);

    ps2 = connected_ps2();
    require(adapter.update(ps2).right_sw == remoter::sw_state::mid);
}

} // namespace

int main()
{
    test_circle_unlocks_and_cross_locks();
    test_unlock_requires_fresh_circle_press_after_startup();
    test_r1_selects_chassis_and_r2_selects_arm_while_unlocked();
    test_locked_shoulders_never_select_a_mode();
    test_link_loss_and_reconnect_clear_unlock();
    test_conflicting_lock_edges_fail_closed_to_locked();
    test_invalid_axes_fail_closed();
    test_dr16_passes_through_and_resets_ps2_unlock();
    return EXIT_SUCCESS;
}
