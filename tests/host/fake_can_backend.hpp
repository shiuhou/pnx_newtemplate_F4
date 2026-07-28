#pragma once

#include "bsp_can.hpp"

#include <array>
#include <cstdint>

namespace host_test::fake_can
{

struct tx_record
{
    bsp::can::bus bus = bsp::can::bus::none;
    std::uint32_t id = 0U;
    std::uint16_t len = 0U;
    std::array<std::uint8_t, 8U> data{};
};

void reset() noexcept;
void set_tick(std::uint32_t tick) noexcept;
void set_recover_status(types::status status) noexcept;
std::uint32_t recover_calls() noexcept;
std::uint32_t transmit_calls() noexcept;
bool observed_recovering_state() noexcept;
tx_record last_transmit() noexcept;

} // namespace host_test::fake_can
