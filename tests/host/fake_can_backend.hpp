#pragma once

#include "bsp_can.hpp"

#include <cstdint>

namespace host_test::fake_can
{

void reset() noexcept;
void set_tick(std::uint32_t tick) noexcept;
void set_recover_status(types::status status) noexcept;
std::uint32_t recover_calls() noexcept;
std::uint32_t transmit_calls() noexcept;
bool observed_recovering_state() noexcept;

} // namespace host_test::fake_can
