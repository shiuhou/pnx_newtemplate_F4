#include "vehicle/slider/control/controller.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::slider::automatic_direction;
using vehicle::slider::controller;
using vehicle::slider::controller_configuration;
using vehicle::slider::controller_input;
using vehicle::slider::limit_state;
using vehicle::slider::operating_mode;

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

remoter::state connected_ps2(float right_x = 0.0F) noexcept
{
    remoter::state remote{};
    remote.offline = false;
    remote.active_source = remoter::source::ps2;
    remote.ps2_link = remoter::ps2_link_state::connected;
    remote.right_x = right_x;
    return remote;
}

controller_configuration config(float direction = 1.0F) noexcept
{
    return {12.0F, 8.0F, 0.08F, direction,
            {400.0F, 0.0F, 0.0F, 1500.0F}};
}

controller_input healthy_input(float right_x = 0.0F) noexcept
{
    controller_input input{};
    input.remote = connected_ps2(right_x);
    input.measured_velocity_rad_s = 0.0F;
    input.motor_online = true;
    input.can_healthy = true;
    input.dt_s = 0.005F;
    return input;
}

void test_manual_requires_center_then_tracks_right_stick() noexcept
{
    controller control{config()};
    auto input = healthy_input(0.5F);
    require(!control.update(input).outputs_enabled);

    input.remote.right_x = 0.0F;
    require(control.update(input).outputs_enabled);

    input.remote.right_x = 0.5F;
    auto output = control.update(input);
    require(output.mode == operating_mode::manual);
    require(output.target_velocity_rad_s == 6.0F);
    require(output.current_raw == 1500);

    input.remote.right_x = -0.25F;
    output = control.update(input);
    require(output.target_velocity_rad_s == -3.0F);
    require(output.current_raw == -1200);
}

void test_invalid_or_unhealthy_input_stops_immediately() noexcept
{
    controller control{config()};
    auto input = healthy_input();
    (void)control.update(input);
    input.remote.right_x = 0.5F;
    require(control.update(input).current_raw != 0);

    input.remote.offline = true;
    require(control.update(input).current_raw == 0);
    input = healthy_input();
    input.motor_online = false;
    require(control.update(input).current_raw == 0);
    input = healthy_input();
    input.can_healthy = false;
    require(control.update(input).current_raw == 0);
    input = healthy_input();
    input.remote.right_x = std::numeric_limits<float>::quiet_NaN();
    require(control.update(input).current_raw == 0);
}

void test_circle_auto_cross_manual_and_cross_wins() noexcept
{
    controller control{config()};
    auto input = healthy_input();
    (void)control.update(input);

    input.remote.ps2_pressed = bit(remoter::ps2_button::circle);
    auto output = control.update(input);
    require(output.mode == operating_mode::automatic);
    require(!output.outputs_enabled);

    input.remote.ps2_pressed = bit(remoter::ps2_button::cross);
    input.remote.right_x = 0.5F;
    output = control.update(input);
    require(output.mode == operating_mode::manual);
    require(!output.outputs_enabled);

    input.remote.ps2_pressed = bit(remoter::ps2_button::circle) |
                               bit(remoter::ps2_button::cross);
    output = control.update(input);
    require(output.mode == operating_mode::manual);
}

void test_automatic_reverses_only_with_ready_limits() noexcept
{
    controller control{config()};
    auto input = healthy_input();
    input.remote.ps2_pressed = bit(remoter::ps2_button::circle);
    require(control.update(input).current_raw == 0);

    input.remote.ps2_pressed = 0U;
    input.limits.ready = true;
    auto output = control.update(input);
    require(output.direction == automatic_direction::right);
    require(output.target_velocity_rad_s == 8.0F);
    require(output.current_raw == 1500);

    input.limits.right_active = true;
    output = control.update(input);
    require(output.direction == automatic_direction::left);
    require(output.current_raw == -1500);

    input.limits = limit_state{true, true, false};
    output = control.update(input);
    require(output.direction == automatic_direction::right);
    require(output.current_raw == 1500);

    input.limits = limit_state{true, true, true};
    output = control.update(input);
    require(!output.outputs_enabled);
    require(output.current_raw == 0);
}

void test_motor_direction_changes_physical_current_sign() noexcept
{
    controller control{config(-1.0F)};
    auto input = healthy_input();
    (void)control.update(input);
    input.remote.right_x = 0.25F;
    const auto output = control.update(input);
    require(output.target_velocity_rad_s == 3.0F);
    require(output.current_raw == -1200);
}

} // namespace

int main()
{
    test_manual_requires_center_then_tracks_right_stick();
    test_invalid_or_unhealthy_input_stops_immediately();
    test_circle_auto_cross_manual_and_cross_wins();
    test_automatic_reverses_only_with_ready_limits();
    test_motor_direction_changes_physical_current_sign();
    return EXIT_SUCCESS;
}
