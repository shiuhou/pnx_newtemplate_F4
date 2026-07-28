#include "fake_pwm_backend.hpp"

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

namespace bsp::pwm::detail
{

bool backend_enabled(channel selected) noexcept
{
    return selected == channel{0U};
}

types::status backend_start(channel selected) noexcept
{
    if (!backend_enabled(selected))
    {
        return types::status::not_configured;
    }
    running = true;
    return types::status::ok;
}

types::status backend_stop(channel selected) noexcept
{
    if (!backend_enabled(selected))
    {
        return types::status::not_configured;
    }
    pulse = 0U;
    running = false;
    return types::status::ok;
}

types::status backend_set_period_us(
    channel selected, std::uint32_t value) noexcept
{
    if (!backend_enabled(selected))
    {
        return types::status::not_configured;
    }
    period = value;
    return types::status::ok;
}

types::status backend_set_pulse_us(
    channel selected, std::uint32_t value) noexcept
{
    if (!backend_enabled(selected))
    {
        return types::status::not_configured;
    }
    pulse = value;
    return types::status::ok;
}

} // namespace bsp::pwm::detail
