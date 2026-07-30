#include "bsp_pwm.hpp"
#include "fake_pwm.hpp"

#include <cstdlib>

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

} // namespace

int main()
{
    constexpr bsp::pwm::channel servo{0U};
    constexpr bsp::pwm::channel invalid{1U};
    host_test::fake_pwm::reset();

    require(bsp::pwm::start(invalid) == types::status::not_configured);
    require(bsp::pwm::set_period_us(servo, 0U) ==
            types::status::invalid_arg);
    require(bsp::pwm::set_pulse_us(servo, 20001U) ==
            types::status::invalid_arg);

    require(bsp::pwm::set_period_us(servo, 20000U) ==
            types::status::ok);
    bsp::pwm::set_period(servo, 0.010F);
    require(host_test::fake_pwm::period_us() == 10000U);
    bsp::pwm::set_period(
        bsp::pwm::channel::tim3_ch4, 0.020F);
    require(host_test::fake_pwm::period_us() == 10000U);
    require(bsp::pwm::set_period_us(servo, 20000U) ==
            types::status::ok);
    require(bsp::pwm::set_pulse_us(servo, 1500U) ==
            types::status::ok);
    require(bsp::pwm::start(servo) == types::status::ok);
    require(host_test::fake_pwm::started());
    require(host_test::fake_pwm::period_us() == 20000U);
    require(host_test::fake_pwm::pulse_us() == 1500U);

    require(bsp::pwm::set_duty(servo, 0.05F) == types::status::ok);
    require(host_test::fake_pwm::pulse_us() == 1000U);

    require(bsp::pwm::stop(servo) == types::status::ok);
    require(!host_test::fake_pwm::started());
    require(host_test::fake_pwm::pulse_us() == 0U);
    return EXIT_SUCCESS;
}
