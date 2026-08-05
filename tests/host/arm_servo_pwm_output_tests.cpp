#include "vehicle/arm/runtime/servo_pwm_output.hpp"

#include "fake_pwm.hpp"
#include "pwm_channels.hpp"

#include <cstdlib>

namespace
{

using host_test::fake_pwm::period_us;
using host_test::fake_pwm::pulse_us;
using host_test::fake_pwm::started;
using vehicle::arm::servo_pwm_output;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

void test_start_update_and_stop_are_fail_closed() noexcept
{
    host_test::fake_pwm::reset();
    servo_pwm_output output{{board::pwm::servo_c2, 20000U}};

    require(output.update(false, 1500U));
    require(!started(board::pwm::servo_c2));
    require(output.pulse_us() == 0U);

    require(output.update(true, 1500U));
    require(started(board::pwm::servo_c2));
    require(period_us() == 20000U);
    require(pulse_us(board::pwm::servo_c2) == 1500U);
    require(output.pulse_us() == 1500U);

    require(output.update(true, 1510U));
    require(pulse_us(board::pwm::servo_c2) == 1510U);

    require(output.update(false, 1510U));
    require(!started(board::pwm::servo_c2));
    require(pulse_us(board::pwm::servo_c2) == 0U);
    require(output.pulse_us() == 0U);
}

void test_invalid_config_and_pulse_never_start_output() noexcept
{
    host_test::fake_pwm::reset();
    servo_pwm_output invalid{{bsp::pwm::none, 20000U}};
    require(!invalid.update(true, 1500U));
    require(!started(board::pwm::servo_c2));

    servo_pwm_output output{{board::pwm::servo_c2, 20000U}};
    require(!output.update(true, 20001U));
    require(!started(board::pwm::servo_c2));
    require(output.pulse_us() == 0U);
}

} // namespace

int main()
{
    test_start_update_and_stop_are_fail_closed();
    test_invalid_config_and_pulse_never_start_output();
    return EXIT_SUCCESS;
}
