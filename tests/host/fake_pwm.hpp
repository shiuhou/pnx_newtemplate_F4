#pragma once

#include "bsp_pwm.hpp"

#include <cstdint>

namespace host_test::fake_pwm
{

void reset() noexcept;
bool started() noexcept;
std::uint32_t period_us() noexcept;
std::uint32_t pulse_us() noexcept;

} // namespace host_test::fake_pwm
