#include "fake_pwm.hpp"

#include <cmath>

namespace
{

bool running = false;
std::uint32_t period = 20000U;
std::uint32_t pulse = 0U;

} // namespace

namespace host_test::fake_pwm
{

void reset() noexcept
{
    running = false;
    period = 20000U;
    pulse = 0U;
}

bool started() noexcept
{
    return running;
}

std::uint32_t period_us() noexcept
{
    return period;
}

std::uint32_t pulse_us() noexcept
{
    return pulse;
}

} // namespace host_test::fake_pwm

namespace bsp::pwm
{

bool is_enabled(channel selected) noexcept
{
    return selected == channel{0U};
}

types::status start(channel selected) noexcept
{
    if (!is_enabled(selected))
    {
        return types::status::not_configured;
    }
    running = true;
    return types::status::ok;
}

types::status stop(channel selected) noexcept
{
    if (!is_enabled(selected))
    {
        return types::status::not_configured;
    }
    pulse = 0U;
    running = false;
    return types::status::ok;
}

types::status set_period_us(
    channel selected, std::uint32_t value) noexcept
{
    if (!is_enabled(selected))
    {
        return types::status::not_configured;
    }
    if (value == 0U || value > 65536U)
    {
        return types::status::invalid_arg;
    }
    period = value;
    return types::status::ok;
}

types::status set_pulse_us(
    channel selected, std::uint32_t value) noexcept
{
    if (!is_enabled(selected))
    {
        return types::status::not_configured;
    }
    if (value > period)
    {
        return types::status::invalid_arg;
    }
    pulse = value;
    return types::status::ok;
}

types::status set_duty(channel selected, float duty_ratio) noexcept
{
    if (!is_enabled(selected))
    {
        return types::status::not_configured;
    }
    if (!std::isfinite(duty_ratio))
    {
        return types::status::invalid_arg;
    }
    if (duty_ratio < 0.0F)
    {
        duty_ratio = 0.0F;
    }
    else if (duty_ratio > 1.0F)
    {
        duty_ratio = 1.0F;
    }
    return set_pulse_us(
        selected,
        static_cast<std::uint32_t>(
            duty_ratio * static_cast<float>(period)));
}

void set_period(channel selected, float period_s) noexcept
{
    if (!is_enabled(selected) || !std::isfinite(period_s) ||
        period_s <= 0.0F || period_s > 0.065536F)
    {
        return;
    }
    (void)set_period_us(
        selected,
        static_cast<std::uint32_t>(period_s * 1000000.0F));
}

} // namespace bsp::pwm
