#pragma once

#include <cstdint>

namespace demo::cboard::can_m2006
{

struct telemetry
{
    std::uint32_t heartbeat = 0U;
    std::uint32_t rx_count = 0U;
    std::uint32_t tx_count = 0U;
    std::uint32_t last_id = 0U;
    std::uint32_t error_count = 0U;
    std::uint32_t drop_count = 0U;
    std::uint32_t fault_epoch = 0U;
    std::uint32_t pulse_count = 0U;
    std::int32_t commanded_current = 0;
    std::uint32_t complete = 0U;
    std::uint32_t faulted = 0U;
};

extern volatile telemetry runtime;

void run() noexcept;

} // namespace demo::cboard::can_m2006
