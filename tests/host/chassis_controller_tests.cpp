#include "vehicle/chassis/control/controller.hpp"
#include "vehicle/chassis/control/velocity_pi.hpp"

// 可執行規格：驗證四輪 PI、方向修正、飽和／重置與 safety gate 交界的零輸出行為。

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace
{

using namespace vehicle::chassis;

constexpr velocity_pi_config pi_config{10.0F, 20.0F, 100.0F, 500.0F};
constexpr controller_configuration base_config{
    {0.10F, 0.20F, 0.15F, 10.0F},
    {0.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {1.0F, 1.0F, 1.0F, 1.0F},
    pi_config,
};
constexpr safety_input healthy_released{true, false, true, true, true};
constexpr safety_input healthy_raised{true, true, true, true, true};
constexpr manual_input manual_released{true, false, 0.0F, 0.0F, 0.0F};
constexpr manual_input manual_raised_zero{true, true, 0.0F, 0.0F, 0.0F};
constexpr manual_input manual_forward{true, true, 0.0F, 1.0F, 0.0F};
constexpr wheel_vector stopped_wheels{{0.0F, 0.0F, 0.0F, 0.0F}};

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool near(float actual, float expected) noexcept
{
    return std::fabs(actual - expected) < 0.0001F;
}

void require_wheels(const wheel_vector& actual,
                    const std::array<float, 4U>& expected) noexcept
{
    for (std::size_t index = 0U; index < expected.size(); ++index)
    {
        require(near(actual.rad_s[index], expected[index]));
    }
}

void require_currents(
    const std::array<std::int16_t, 4U>& actual,
    const std::array<std::int16_t, 4U>& expected) noexcept
{
    require(actual == expected);
}

void arm(controller& control) noexcept
{
    const auto released = control.update(
        manual_released, stopped_wheels, healthy_released, 0.005F);
    require(released.state == safety_state::disabled);
    require_currents(released.motor_current_raw, {0, 0, 0, 0});

    const auto raised = control.update(
        manual_raised_zero, stopped_wheels, healthy_raised, 0.005F);
    require(raised.state == safety_state::armed);
    require_currents(raised.motor_current_raw, {0, 0, 0, 0});
}

void seed_integral(velocity_pi& pi) noexcept
{
    require(pi.update(1.0F, 0.0F, 1.0F) == 30);
    require(pi.update(0.0F, 0.0F, 0.005F) == 20);
}

void test_velocity_pi_nominal_and_limits() noexcept
{
    velocity_pi pi{pi_config};
    require(pi.update(0.0F, 0.0F, 0.005F) == 0);
    require(pi.update(10.0F, 0.0F, 0.005F) == 101);

    velocity_pi positive{pi_config};
    velocity_pi negative{pi_config};
    require(positive.update(1000.0F, 0.0F, 0.005F) == 500);
    require(negative.update(-1000.0F, 0.0F, 0.005F) == -500);

    velocity_pi bounded_integral{pi_config};
    for (int step = 0; step < 20; ++step)
    {
        require(bounded_integral.update(1.0F, 0.0F, 1.0F) <= 110);
    }
    require(bounded_integral.update(0.0F, 0.0F, 0.005F) == 100);

    velocity_pi anti_windup{pi_config};
    for (int step = 0; step < 20; ++step)
    {
        require(anti_windup.update(1000.0F, 0.0F, 1.0F) == 500);
    }
    require(anti_windup.update(0.0F, 0.0F, 0.005F) == 0);

    bounded_integral.reset();
    require(bounded_integral.update(0.0F, 0.0F, 0.005F) == 0);
}

void test_velocity_pi_invalid_input_resets() noexcept
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();

    for (const float invalid_dt : {0.0F, -0.005F, infinity, nan})
    {
        velocity_pi pi{pi_config};
        seed_integral(pi);
        require(pi.update(0.0F, 0.0F, invalid_dt) == 0);
        require(pi.update(0.0F, 0.0F, 0.005F) == 0);
    }

    velocity_pi invalid_target{pi_config};
    seed_integral(invalid_target);
    require(invalid_target.update(nan, 0.0F, 0.005F) == 0);
    require(invalid_target.update(0.0F, 0.0F, 0.005F) == 0);

    velocity_pi invalid_measurement{pi_config};
    seed_integral(invalid_measurement);
    require(invalid_measurement.update(0.0F, infinity, 0.005F) == 0);
    require(invalid_measurement.update(0.0F, 0.0F, 0.005F) == 0);
}

void test_controller_configuration_validation() noexcept
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    require(valid(base_config));

    auto config = base_config;
    config.geometry.wheel_radius_m = 0.0F;
    require(!valid(config));
    config = base_config;
    config.geometry.half_wheelbase_m = -0.1F;
    require(!valid(config));
    config = base_config;
    config.geometry.max_wheel_rad_s = nan;
    require(!valid(config));

    config = base_config;
    config.manual.deadband = 1.0F;
    require(!valid(config));
    config = base_config;
    config.manual.max_vx_mps = 0.0F;
    require(!valid(config));
    config = base_config;
    config.manual.yaw_sign = 0.0F;
    require(!valid(config));

    config = base_config;
    config.motor_direction[2] = 0.0F;
    require(!valid(config));
    config = base_config;
    config.motor_direction[1] = nan;
    require(!valid(config));

    config = base_config;
    config.pi.kp = -1.0F;
    require(!valid(config));
    config = base_config;
    config.pi.ki_per_s = -1.0F;
    require(!valid(config));
    config = base_config;
    config.pi.kp = 0.0F;
    config.pi.ki_per_s = 0.0F;
    require(!valid(config));
    config = base_config;
    config.pi.kp = 0.0F;
    config.pi.ki_per_s = 1.0F;
    config.pi.integral_limit_raw = 0.0F;
    require(!valid(config));
    config = base_config;
    config.pi.integral_limit_raw = -1.0F;
    require(!valid(config));
    config = base_config;
    config.pi.current_limit_raw = 0.0F;
    require(!valid(config));
    config = base_config;
    config.pi.current_limit_raw =
        static_cast<float>(std::numeric_limits<std::int16_t>::max()) + 1.0F;
    require(!valid(config));
    config = base_config;
    config.pi.current_limit_raw = 10000.0F;
    require(valid(config));
    config.pi.current_limit_raw = 10001.0F;
    require(!valid(config));
    config = base_config;
    config.pi.kp = nan;
    require(!valid(config));

    controller invalid{config};
    const auto output = invalid.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require(output.state == safety_state::disabled);
    require_wheels(output.wheel_target_rad_s, {0.0F, 0.0F, 0.0F, 0.0F});
    require_currents(output.motor_current_raw, {0, 0, 0, 0});
}

void test_controller_targets_and_directions() noexcept
{
    controller forward{base_config};
    arm(forward);
    const auto forward_output = forward.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require_wheels(forward_output.wheel_target_rad_s,
                   {10.0F, 10.0F, 10.0F, 10.0F});
    require_currents(forward_output.motor_current_raw, {101, 101, 101, 101});

    controller strafe{base_config};
    arm(strafe);
    const auto strafe_output = strafe.update(
        {true, true, 1.0F, 0.0F, 0.0F}, stopped_wheels,
        healthy_raised, 0.005F);
    require_wheels(strafe_output.wheel_target_rad_s,
                   {-10.0F, 10.0F, 10.0F, -10.0F});

    controller rotation{base_config};
    arm(rotation);
    const auto rotation_output = rotation.update(
        {true, true, 0.0F, 0.0F, 1.0F}, stopped_wheels,
        healthy_raised, 0.005F);
    require_wheels(rotation_output.wheel_target_rad_s,
                   {-3.5F, 3.5F, -3.5F, 3.5F});

    controller mixed{base_config};
    arm(mixed);
    const auto mixed_output = mixed.update(
        {true, true, 0.5F, 1.0F, 0.0F}, stopped_wheels,
        healthy_raised, 0.005F);
    require_wheels(mixed_output.wheel_target_rad_s,
                   {10.0F / 3.0F, 10.0F, 10.0F, 10.0F / 3.0F});
    require(near(mixed_output.wheel_target_rad_s.rad_s[1] /
                     mixed_output.wheel_target_rad_s.rad_s[0],
                 3.0F));

    auto directed_config = base_config;
    directed_config.motor_direction = {-1.0F, 1.0F, -1.0F, 1.0F};
    controller directed{directed_config};
    arm(directed);
    constexpr wheel_vector signed_raw_feedback{{-2.0F, 2.0F, 3.0F, -3.0F}};
    const auto directed_output = directed.update(
        manual_forward, signed_raw_feedback, healthy_raised, 0.005F);
    require_wheels(directed_output.wheel_target_rad_s,
                   {10.0F, 10.0F, 10.0F, 10.0F});
    require_currents(directed_output.motor_current_raw,
                     {-81, 81, -131, 131});
}

void test_controller_atomic_derived_error_preflight() noexcept
{
    constexpr float maximum = std::numeric_limits<float>::max();
    auto extreme_config = base_config;
    extreme_config.geometry = {1.0F, 0.20F, 0.15F, maximum};
    extreme_config.manual =
        {0.0F, maximum, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F};
    extreme_config.pi = {0.0F, 20.0F, 100.0F, 500.0F};

    controller control{extreme_config};
    arm(control);
    constexpr wheel_vector seed_feedback{{-1.0F, -1.0F, -1.0F, -1.0F}};
    const auto seeded = control.update(
        manual_raised_zero, seed_feedback, healthy_raised, 0.05F);
    require_currents(seeded.motor_current_raw, {1, 1, 1, 1});

    const wheel_vector overflow_feedback{{-maximum, maximum, maximum, maximum}};
    const auto rejected = control.update(
        manual_forward, overflow_feedback, healthy_raised, 0.005F);
    require(rejected.state == safety_state::armed);
    require_wheels(rejected.wheel_target_rad_s,
                   {maximum, maximum, maximum, maximum});
    require_currents(rejected.motor_current_raw, {0, 0, 0, 0});

    const auto after_reset = control.update(
        manual_raised_zero, stopped_wheels, healthy_raised, 0.005F);
    require_currents(after_reset.motor_current_raw, {0, 0, 0, 0});
}

void test_controller_invalid_step_resets_all_pi() noexcept
{
    auto integral_config = base_config;
    integral_config.pi = {0.0F, 20.0F, 100.0F, 500.0F};

    controller bad_dt{integral_config};
    arm(bad_dt);
    const auto dt_seeded = bad_dt.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require_currents(dt_seeded.motor_current_raw, {1, 1, 1, 1});
    const auto invalid_dt = bad_dt.update(
        manual_forward, stopped_wheels, healthy_raised, 0.0F);
    require(invalid_dt.state == safety_state::armed);
    require_currents(invalid_dt.motor_current_raw, {0, 0, 0, 0});
    const auto after_invalid_dt = bad_dt.update(
        manual_raised_zero, stopped_wheels, healthy_raised, 0.005F);
    require_currents(after_invalid_dt.motor_current_raw, {0, 0, 0, 0});

    controller bad_measurement{integral_config};
    arm(bad_measurement);
    const auto measurement_seeded = bad_measurement.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require_currents(measurement_seeded.motor_current_raw, {1, 1, 1, 1});
    wheel_vector one_nan = stopped_wheels;
    one_nan.rad_s[2] = std::numeric_limits<float>::quiet_NaN();
    const auto rejected = bad_measurement.update(
        manual_forward, one_nan, healthy_raised, 0.005F);
    require(rejected.state == safety_state::armed);
    require_currents(rejected.motor_current_raw, {0, 0, 0, 0});
    const auto after_invalid_measurement = bad_measurement.update(
        manual_raised_zero, stopped_wheels, healthy_raised, 0.005F);
    require_currents(after_invalid_measurement.motor_current_raw, {0, 0, 0, 0});
}

void test_controller_malformed_manual_input_inhibits_output() noexcept
{
    controller control{base_config};
    arm(control);
    manual_input malformed = manual_forward;
    malformed.left_y = std::numeric_limits<float>::quiet_NaN();
    constexpr wheel_vector moving{{1.0F, 1.0F, 1.0F, 1.0F}};

    const auto rejected = control.update(
        malformed, moving, healthy_raised, 0.005F);
    require(rejected.state == safety_state::fault_latched);
    require_currents(rejected.motor_current_raw, {0, 0, 0, 0});
}

void test_controller_fault_and_disarm_reset() noexcept
{
    controller faulted{base_config};
    arm(faulted);
    const auto moving = faulted.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require_currents(moving.motor_current_raw, {101, 101, 101, 101});

    const auto fault = faulted.update(
        manual_forward, stopped_wheels,
        {true, true, true, false, true}, 0.005F);
    require(fault.state == safety_state::fault_latched);
    require_currents(fault.motor_current_raw, {0, 0, 0, 0});

    auto integral_config = base_config;
    integral_config.pi = {0.0F, 20.0F, 100.0F, 500.0F};
    controller reset_on_disarm{integral_config};
    arm(reset_on_disarm);
    const auto first_integral = reset_on_disarm.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require_currents(first_integral.motor_current_raw, {1, 1, 1, 1});
    const auto accumulated = reset_on_disarm.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require_currents(accumulated.motor_current_raw, {2, 2, 2, 2});

    const auto disarmed = reset_on_disarm.update(
        manual_released, stopped_wheels, healthy_released, 0.005F);
    require(disarmed.state == safety_state::disabled);
    require_currents(disarmed.motor_current_raw, {0, 0, 0, 0});

    const auto rearmed = reset_on_disarm.update(
        manual_raised_zero, stopped_wheels, healthy_raised, 0.005F);
    require(rearmed.state == safety_state::armed);
    require_currents(rearmed.motor_current_raw, {0, 0, 0, 0});
    const auto after_rearm = reset_on_disarm.update(
        manual_forward, stopped_wheels, healthy_raised, 0.005F);
    require_currents(after_rearm.motor_current_raw, {1, 1, 1, 1});
}

void test_controller_explicit_reset_is_release_gated_and_resets_pi() noexcept
{
    auto integral_config = base_config;
    integral_config.pi = {0.0F, 20.0F, 100.0F, 500.0F};
    controller control{integral_config};
    arm(control);

    require_currents(
        control.update(manual_forward, stopped_wheels,
                       healthy_raised, 0.005F).motor_current_raw,
        {1, 1, 1, 1});
    require_currents(
        control.update(manual_forward, stopped_wheels,
                       healthy_raised, 0.005F).motor_current_raw,
        {2, 2, 2, 2});
    control.reset();
    const auto pi_reset = control.update(
        manual_raised_zero, stopped_wheels, healthy_raised, 0.005F);
    require(pi_reset.state == safety_state::armed);
    require_currents(pi_reset.motor_current_raw, {0, 0, 0, 0});

    const auto fault = control.update(
        manual_forward, stopped_wheels,
        {false, true, true, true, true}, 0.005F);
    require(fault.state == safety_state::fault_latched);

    require(control.update(
                {false, false, 0.0F, 0.0F, 0.0F}, stopped_wheels,
                {false, false, true, true, true}, 0.005F).state ==
            safety_state::fault_latched);
    control.reset();
    require(control.update(
                manual_raised_zero, stopped_wheels,
                healthy_raised, 0.005F).state ==
            safety_state::fault_latched);

    require(control.update(
                manual_released, stopped_wheels,
                healthy_released, 0.005F).state ==
            safety_state::fault_latched);
    control.reset();
    require(control.update(
                manual_released, stopped_wheels,
                healthy_released, 0.005F).state ==
            safety_state::disabled);
    require(control.update(
                manual_raised_zero, stopped_wheels,
                healthy_raised, 0.005F).state ==
            safety_state::armed);
}

} // namespace

int main()
{
    test_velocity_pi_nominal_and_limits();
    test_velocity_pi_invalid_input_resets();
    test_controller_configuration_validation();
    test_controller_targets_and_directions();
    test_controller_atomic_derived_error_preflight();
    test_controller_invalid_step_resets_all_pi();
    test_controller_malformed_manual_input_inhibits_output();
    test_controller_fault_and_disarm_reset();
    test_controller_explicit_reset_is_release_gated_and_resets_pi();
    return EXIT_SUCCESS;
}
