#include "fake_pwm.hpp"

// 鏡像 pnx_bsp/pwm/src/bsp_pwm.cpp 的可觀察契約：
// 共用 TIM1 時基（單一週期）+ 每通道獨立脈寬／start/stop。
// host 沒有 HAL stub，故以 fake 復刻契約；pwm_channels.hpp 是唯一的通道來源。

#include <array>
#include <cmath>
#include <cstddef>

namespace
{

constexpr std::uint32_t default_period_us = 20000U;

std::uint32_t shared_period = default_period_us;
std::array<bool, host_test::fake_pwm::channel_count> running{};
std::array<std::uint32_t, host_test::fake_pwm::channel_count> pulses{};

constexpr std::array<bsp::pwm::channel, host_test::fake_pwm::channel_count>
    logical_channels{{
        board::pwm::servo_c2,
        board::pwm::servo_pe9,
        board::pwm::servo_pe13,
        board::pwm::servo_pe14,
    }};

bool index_of(bsp::pwm::channel selected, std::size_t& out) noexcept
{
    for (std::size_t i = 0U; i < logical_channels.size(); ++i)
    {
        if (logical_channels[i] == selected)
        {
            out = i;
            return true;
        }
    }
    return false;
}

bool any_running() noexcept
{
    for (const bool started : running)
    {
        if (started)
        {
            return true;
        }
    }
    return false;
}

} // namespace

namespace host_test::fake_pwm
{

void reset() noexcept
{
    shared_period = default_period_us;
    running.fill(false);
    pulses.fill(0U);
}

std::uint32_t period_us() noexcept
{
    return shared_period;
}

bool started(bsp::pwm::channel selected) noexcept
{
    std::size_t index = 0U;
    return index_of(selected, index) && running[index];
}

std::uint32_t pulse_us(bsp::pwm::channel selected) noexcept
{
    std::size_t index = 0U;
    return index_of(selected, index) ? pulses[index] : 0U;
}

} // namespace host_test::fake_pwm

namespace bsp::pwm
{

bool is_enabled(channel selected) noexcept
{
    std::size_t index = 0U;
    return index_of(selected, index);
}

types::status start(channel selected) noexcept
{
    std::size_t index = 0U;
    if (!index_of(selected, index))
    {
        return types::status::not_configured;
    }
    running[index] = true;
    return types::status::ok;
}

types::status stop(channel selected) noexcept
{
    std::size_t index = 0U;
    if (!index_of(selected, index))
    {
        return types::status::not_configured;
    }
    pulses[index] = 0U;
    running[index] = false;
    if (!any_running())
    {
        shared_period = default_period_us;
    }
    return types::status::ok;
}

types::status set_period_us(
    channel selected, std::uint32_t value) noexcept
{
    std::size_t index = 0U;
    if (!index_of(selected, index))
    {
        return types::status::not_configured;
    }
    if (value == 0U || value > 65536U)
    {
        return types::status::invalid_arg;
    }
    for (const std::uint32_t pulse : pulses)
    {
        if (pulse > value)
        {
            return types::status::invalid_arg;
        }
    }
    shared_period = value;
    return types::status::ok;
}

types::status set_pulse_us(
    channel selected, std::uint32_t value) noexcept
{
    std::size_t index = 0U;
    if (!index_of(selected, index))
    {
        return types::status::not_configured;
    }
    if (value > shared_period)
    {
        return types::status::invalid_arg;
    }
    pulses[index] = value;
    return types::status::ok;
}

types::status set_duty(channel selected, float duty_ratio) noexcept
{
    std::size_t index = 0U;
    if (!index_of(selected, index))
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
            duty_ratio * static_cast<float>(shared_period)));
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
