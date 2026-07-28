#pragma once

#include <cstdint>

namespace demo::cboard::dbus_rx
{

struct telemetry
{
    std::uint32_t heartbeat = 0U;
    std::uint32_t update_count = 0U;
    std::uint32_t init_failed = 0U;
    std::uint32_t offline = 1U;
    float right_x = 0.0F;
    float right_y = 0.0F;
    float left_x = 0.0F;
    float left_y = 0.0F;
};

extern "C" volatile telemetry pnx_f407_dbus_telemetry;

void run() noexcept;

} // namespace demo::cboard::dbus_rx
