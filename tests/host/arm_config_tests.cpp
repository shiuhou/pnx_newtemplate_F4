#include "vehicle/arm/runtime/arm_config.hpp"

#include "pwm_channels.hpp"

// 可執行規格：方向確認 image 必須同時限制 J1 位置外環、速度內環、
// 位置窗口、方向符號與 5 ms 控制週期。

#include <cstdlib>
#include <limits>

namespace
{

using vehicle::arm::arm_configuration;
using vehicle::arm::configuration;
using vehicle::arm::valid;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

configuration healthy_config() noexcept
{
    return {
        {2.0F, 0.5F, 5.0F},
        {120.0F, 0.0F, 0.0F, 600.0F},
        1.5F,
        -0.9F,
        0.9F,
        0.08F,
        {350.0F, 0.05F, 0.15F, 2.0F},
        100.0F,
        0.0F,
        0.0F,
        1.0F,
        {
            0.08F,
            {{
                {board::pwm::servo_c2, 1500.0F, 1500.0F, 1600.0F, 100.0F},
                {board::pwm::servo_pe13, 1500.0F, 1000.0F, 2000.0F, 500.0F},
                {board::pwm::servo_pe14, 1500.0F, 1200.0F, 1800.0F, 300.0F},
            }},
        },
        0.005F,
    };
}

void test_accepts_healthy_config() noexcept
{
    require(valid(healthy_config()));
    const auto bringup = arm_configuration();
    require(valid(bringup));
    require(bringup.j1_velocity.kp == 7000.0F);
    require(bringup.j1_velocity.current_limit_raw == 4000.0F);
    require(bringup.j1_position.target_velocity_limit_rad_s == 1.2F);
    require(bringup.j1_manual_position_rate_rad_per_s == 1.2F);
    require(bringup.j1_stall.current_threshold_raw == 3500.0F);
    require(bringup.j1_stall.timeout_s == 2.0F);
    require(bringup.j1_position_min_rad == 0.0F);
    require(bringup.j1_position_max_rad > 3.14F);
    require(bringup.j1_position_max_rad < 3.15F);
    require(bringup.j1_gravity_amplitude_raw == 500.0F);
    require(bringup.j1_gravity_phase_rad == 0.0F);
    require(bringup.j1_gravity_bias_raw == 0.0F);
    require(bringup.j1_motor_direction == -1.0F);
    const auto& j2 = bringup.servos.axes[
        vehicle::arm::servo_index(vehicle::arm::servo_axis::j2_pitch)];
    require(j2.channel == board::pwm::servo_c2);
    require(j2.center_pulse_us == 300.0F);
    require(j2.min_pulse_us == 300.0F);
    require(j2.max_pulse_us == 2800.0F);
    require(j2.max_rate_us_per_s == 1000.0F);
    const auto& gripper = bringup.servos.axes[
        vehicle::arm::servo_index(vehicle::arm::servo_axis::gripper)];
    require(gripper.channel == board::pwm::servo_pe14);
    require(gripper.center_pulse_us == 900.0F);
    require(gripper.min_pulse_us == 900.0F);
    require(gripper.max_pulse_us == 2000.0F);
    require(gripper.max_rate_us_per_s == 500.0F);
}

void test_rejects_invalid_velocity_pi_config() noexcept
{
    auto config = healthy_config();
    config.j1_velocity.current_limit_raw = 0.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_velocity.kp = -1.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_velocity.current_limit_raw = 10001.0F;
    require(!valid(config));
}

void test_rejects_invalid_position_pid_config() noexcept
{
    auto config = healthy_config();
    config.j1_position = {0.0F, 0.0F, 5.0F};
    require(!valid(config));

    config = healthy_config();
    config.j1_position = {-1.0F, 0.0F, 5.0F};
    require(!valid(config));

    config = healthy_config();
    config.j1_position = {1.0F, -0.1F, 5.0F};
    require(!valid(config));

    config = healthy_config();
    config.j1_position = {1.0F, 0.0F, 0.0F};
    require(!valid(config));
}

void test_rejects_invalid_manual_position_rate() noexcept
{
    auto config = healthy_config();
    config.j1_manual_position_rate_rad_per_s = 0.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_manual_position_rate_rad_per_s = -1.0F;
    require(!valid(config));
}

void test_rejects_invalid_control_period() noexcept
{
    auto config = healthy_config();
    config.control_period_s = 0.0F;
    require(!valid(config));

    config = healthy_config();
    config.control_period_s = -0.005F;
    require(!valid(config));
}

void test_rejects_invalid_position_window_deadband_and_direction() noexcept
{
    auto config = healthy_config();
    config.j1_position_min_rad = 1.0F;
    config.j1_position_max_rad = 1.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_manual_deadband = 1.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_stall.timeout_s = 0.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_stall.current_threshold_raw = 601.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_motor_direction = 0.5F;
    require(!valid(config));

    config = healthy_config();
    config.j1_gravity_amplitude_raw = 601.0F;
    config.j1_velocity.current_limit_raw = 600.0F;
    require(!valid(config));
}

void test_rejects_non_finite_values() noexcept
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();

    auto config = healthy_config();
    config.j1_position.kp = nan;
    require(!valid(config));

    config = healthy_config();
    config.j1_velocity.current_limit_raw = infinity;
    require(!valid(config));

    config = healthy_config();
    config.j1_position_min_rad = nan;
    require(!valid(config));

    config = healthy_config();
    config.j1_gravity_phase_rad = infinity;
    require(!valid(config));
}

} // namespace

int main()
{
    test_accepts_healthy_config();
    test_rejects_invalid_position_pid_config();
    test_rejects_invalid_velocity_pi_config();
    test_rejects_invalid_manual_position_rate();
    test_rejects_invalid_control_period();
    test_rejects_invalid_position_window_deadband_and_direction();
    test_rejects_non_finite_values();
    return EXIT_SUCCESS;
}
