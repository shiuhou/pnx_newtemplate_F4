#pragma once

#include <cstdint>

namespace demo::cboard::pwm_a2
{

struct telemetry
{
    std::uint32_t heartbeat = 0U;
    std::uint32_t step_count = 0U;
    std::uint32_t pulse_us = 0U;
    std::uint32_t complete = 0U;
    std::uint32_t faulted = 0U;
    std::uint32_t output_enabled = 0U;
};

extern volatile telemetry runtime;

void run() noexcept;

} // namespace demo::cboard::pwm_a2
