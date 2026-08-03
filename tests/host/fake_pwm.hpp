#pragma once

#include "bsp_pwm.hpp"
#include "pwm_channels.hpp"

#include <cstddef>
#include <cstdint>

namespace host_test::fake_pwm
{

inline constexpr std::size_t channel_count =
    board::pwm::servo_channel_count;

void reset() noexcept;

// 週期由共用 TIM1 時基決定（全通道同一 ARR）；脈寬與 start/stop 為每通道獨立。
std::uint32_t period_us() noexcept;
bool started(bsp::pwm::channel selected) noexcept;
std::uint32_t pulse_us(bsp::pwm::channel selected) noexcept;

} // namespace host_test::fake_pwm
