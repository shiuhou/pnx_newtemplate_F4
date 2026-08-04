#include "vehicle/arm/control/servo_map.hpp"

#include "pwm_channels.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::arm::servo_axis;
using vehicle::arm::servo_count;
using vehicle::arm::servo_index;
using vehicle::arm::servo_map;
using vehicle::arm::servo_map_config;
using vehicle::arm::servo_map_input;
using vehicle::arm::valid;

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

servo_map_config baseline_config() noexcept
{
    return {
        0.05F,
        {{
            {board::pwm::servo_pe9, 1500.0F, 1100.0F, 1900.0F, 400.0F},
            {board::pwm::servo_pe13, 1500.0F, 1000.0F, 2000.0F, 500.0F},
            {board::pwm::servo_pe14, 1500.0F, 1200.0F, 1800.0F, 300.0F},
        }},
    };
}

void require_centered(const vehicle::arm::servo_map_output& output) noexcept
{
    require(output.pulse_us[servo_index(servo_axis::j2_pitch)] == 1500U);
    require(output.pulse_us[servo_index(servo_axis::j3_yaw)] == 1500U);
    require(output.pulse_us[servo_index(servo_axis::gripper)] == 1500U);
}

void test_validates_config() noexcept
{
    auto config = baseline_config();
    require(valid(config));

    config.deadband = 1.0F;
    require(!valid(config));
    config = baseline_config();
    config.axes[servo_index(servo_axis::j3_yaw)].channel = bsp::pwm::none;
    require(!valid(config));
    config = baseline_config();
    config.axes[servo_index(servo_axis::j3_yaw)].center_pulse_us = 2500.0F;
    require(!valid(config));
    config = baseline_config();
    config.axes[servo_index(servo_axis::j3_yaw)].max_rate_us_per_s = 0.0F;
    require(!valid(config));
}

void test_deadband_and_increment() noexcept
{
    servo_map map{baseline_config()};
    require_centered(map.update({true, 0.0F}, 0.01F));
    require_centered(map.update({true, 0.02F}, 0.01F));

    const auto moved = map.update({true, 0.55F}, 0.10F);
    require(moved.pulse_us[servo_index(servo_axis::j2_pitch)] == 1500U);
    require(moved.pulse_us[servo_index(servo_axis::gripper)] == 1500U);
    require(moved.pulse_us[servo_index(servo_axis::j3_yaw)] > 1500U);
    require(moved.pulse_us[servo_index(servo_axis::j3_yaw)] == 1526U);

    const auto reversed = map.update({true, -0.55F}, 0.10F);
    require(reversed.pulse_us[servo_index(servo_axis::j3_yaw)] == 1500U);
}

void test_rate_limit_and_clamp() noexcept
{
    servo_map map{baseline_config()};

    auto output = map.update({true, 1.0F}, 1.50F);
    require(output.pulse_us[servo_index(servo_axis::j3_yaw)] == 2000U);

    output = map.update({true, -1.0F}, 3.00F);
    require(output.pulse_us[servo_index(servo_axis::j3_yaw)] == 1000U);
}

void test_fail_closed_reset() noexcept
{
    servo_map map{baseline_config()};
    (void)map.update({true, 0.80F}, 0.20F);

    require_centered(map.update({false, 0.80F}, 0.20F));
    require_centered(map.update({true,
                                 std::numeric_limits<float>::quiet_NaN()},
                                0.20F));
    require_centered(map.update({true, 0.80F}, 0.0F));

    auto bad = baseline_config();
    bad.axes[servo_index(servo_axis::j3_yaw)].max_rate_us_per_s =
        std::numeric_limits<float>::infinity();
    servo_map invalid{bad};
    require_centered(invalid.update({true, 1.0F}, 0.10F));
}

} // namespace

int main()
{
    static_assert(servo_count == 3U);
    test_validates_config();
    test_deadband_and_increment();
    test_rate_limit_and_clamp();
    test_fail_closed_reset();
    return EXIT_SUCCESS;
}
