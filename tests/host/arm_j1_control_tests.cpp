#include "vehicle/arm/control/gravity_feedforward.hpp"
#include "vehicle/arm/control/j1_manual_command.hpp"
#include "vehicle/arm/control/j1_stall_guard.hpp"
#include "vehicle/arm/control/j1_zero_reference.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::arm::combine_current_raw;
using vehicle::arm::gravity_current_raw;
using vehicle::arm::j1_manual_command;
using vehicle::arm::j1_manual_command_input;
using vehicle::arm::j1_stall_direction;
using vehicle::arm::j1_stall_guard;
using vehicle::arm::j1_zero_reference;

constexpr float pi = 3.14159265358979323846F;
constexpr float tolerance = 1.0e-4F;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool near(float actual, float expected) noexcept
{
    return std::fabs(actual - expected) <= tolerance;
}

j1_manual_command command() noexcept
{
    return j1_manual_command{{3.0F, 0.2F, 1.2F},
                             1.2F, 0.0F, pi, 0.08F};
}

j1_manual_command_input input(float axis, float position = 0.4F) noexcept
{
    return {true, axis, position, 0.0F, 0.005F};
}

j1_stall_guard stall_guard() noexcept
{
    return j1_stall_guard{{3500.0F, 0.05F, 0.15F, 2.0F}};
}

void test_first_enable_seeds_target_and_center_holds() noexcept
{
    auto control = command();
    auto output = control.update(input(0.0F));
    require(near(output.target_position_rad, 0.4F));
    require(near(output.target_velocity_rad_s, 0.0F));

    output = control.update(input(1.0F));
    require(near(output.target_position_rad, 0.406F));
    require(output.target_velocity_rad_s > 0.0F);

    output = control.update(input(0.04F));
    require(near(output.target_position_rad, 0.406F));

    output = control.update(input(-1.0F));
    require(near(output.target_position_rad, 0.4F));
    require(output.target_velocity_rad_s <= 0.0F);
}

void test_soft_limits_and_reset_reseed() noexcept
{
    auto control = command();
    auto output = control.update(input(1.0F, pi - 0.001F));
    require(near(output.target_position_rad, pi));
    output = control.update(input(1.0F, pi));
    require(near(output.target_position_rad, pi));

    control.reset();
    output = control.update(input(-1.0F, 0.001F));
    require(near(output.target_position_rad, 0.0F));

    output = control.update({false, 1.0F, 0.7F, 0.0F, 0.005F});
    require(near(output.target_velocity_rad_s, 0.0F));
    output = control.update(input(0.0F, 0.7F));
    require(near(output.target_position_rad, 0.7F));
    require(near(output.target_velocity_rad_s, 0.0F));
}

void test_invalid_manual_input_fails_closed() noexcept
{
    auto control = command();
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    auto output = control.update({true, nan, 0.4F, 0.0F, 0.005F});
    require(near(output.target_position_rad, 0.0F));
    require(near(output.target_velocity_rad_s, 0.0F));

    output = control.update({true, 0.0F, 0.4F, 0.0F, 0.0F});
    require(near(output.target_position_rad, 0.0F));
    require(near(output.target_velocity_rad_s, 0.0F));
}

void test_gravity_feedforward_shape_and_current_limit() noexcept
{
    require(near(gravity_current_raw(0.0F, 500.0F, 0.0F, 0.0F),
                 500.0F));
    require(near(gravity_current_raw(pi / 2.0F, 500.0F, 0.0F, 0.0F),
                 0.0F));
    require(near(gravity_current_raw(pi, 500.0F, 0.0F, 0.0F),
                 -500.0F));
    require(near(gravity_current_raw(0.0F, 500.0F, pi / 2.0F, 25.0F),
                 25.0F));

    require(combine_current_raw(3000, 500.0F, 4000.0F) == 3500);
    require(combine_current_raw(3900, 500.0F, 4000.0F) == 4000);
    require(combine_current_raw(-3900, -500.0F, 4000.0F) == -4000);

    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    require(near(gravity_current_raw(nan, 500.0F, 0.0F, 0.0F), 0.0F));
    require(combine_current_raw(100, nan, 4000.0F) == 0);
    require(combine_current_raw(100, 10.0F, 0.0F) == 0);
}

void test_zero_reference_captures_only_once_per_boot() noexcept
{
    j1_zero_reference reference;
    require(!reference.captured());
    require(near(reference.logical_position(0.8F), 0.0F));
    require(reference.capture(0.8F));
    require(reference.captured());
    require(near(reference.logical_position(0.8F), 0.0F));
    require(near(reference.logical_position(1.3F), 0.5F));
    require(reference.capture(1.1F));
    require(near(reference.logical_position(1.3F), 0.5F));

    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    j1_zero_reference invalid;
    require(!invalid.capture(nan));
    require(!invalid.captured());
    require(near(reference.logical_position(nan), 0.0F));
}

void test_stall_blocks_only_the_loaded_direction_after_two_seconds() noexcept
{
    auto guard = stall_guard();
    vehicle::arm::j1_stall_guard_input input{
        true, 1.0F, 0.6F, 0.4F, 0.0F, 3600, 0.005F};

    vehicle::arm::j1_stall_guard_output output{};
    for (std::uint32_t cycle = 0U; cycle < 399U; ++cycle)
    {
        output = guard.update(input);
        require(!output.hold_target);
        require(output.blocked_direction == j1_stall_direction::none);
        require(near(output.allowed_manual_axis, 1.0F));
    }

    output = guard.update(input);
    require(output.hold_target);
    require(output.blocked_direction == j1_stall_direction::positive);
    require(near(output.allowed_manual_axis, 0.0F));
    require(near(output.stall_elapsed_s, 2.0F));

    output = guard.update(input);
    require(!output.hold_target);
    require(output.blocked_direction == j1_stall_direction::positive);
    require(near(output.allowed_manual_axis, 0.0F));

    input.manual_axis = 0.0F;
    output = guard.update(input);
    require(output.blocked_direction == j1_stall_direction::positive);
    require(near(output.allowed_manual_axis, 0.0F));

    input.manual_axis = -1.0F;
    output = guard.update(input);
    require(output.blocked_direction == j1_stall_direction::none);
    require(near(output.allowed_manual_axis, -1.0F));
}

void test_stall_timer_resets_when_motion_resumes_and_on_disable() noexcept
{
    auto guard = stall_guard();
    vehicle::arm::j1_stall_guard_input input{
        true, -1.0F, 0.2F, 0.4F, 0.0F, -3600, 0.005F};

    vehicle::arm::j1_stall_guard_output output{};
    for (std::uint32_t cycle = 0U; cycle < 200U; ++cycle)
    {
        output = guard.update(input);
    }
    require(output.stall_elapsed_s > 0.99F);
    require(output.stall_elapsed_s < 1.01F);

    input.measured_velocity_rad_s = -0.2F;
    output = guard.update(input);
    require(near(output.stall_elapsed_s, 0.0F));
    require(output.blocked_direction == j1_stall_direction::none);

    input.measured_velocity_rad_s = 0.0F;
    input.enabled = false;
    output = guard.update(input);
    require(near(output.stall_elapsed_s, 0.0F));
    require(output.blocked_direction == j1_stall_direction::none);
    require(near(output.allowed_manual_axis, 0.0F));
}

} // namespace

int main()
{
    test_first_enable_seeds_target_and_center_holds();
    test_soft_limits_and_reset_reseed();
    test_invalid_manual_input_fails_closed();
    test_gravity_feedforward_shape_and_current_limit();
    test_zero_reference_captures_only_once_per_boot();
    test_stall_blocks_only_the_loaded_direction_after_two_seconds();
    test_stall_timer_resets_when_motion_resumes_and_on_disable();
    return EXIT_SUCCESS;
}
