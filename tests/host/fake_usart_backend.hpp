#pragma once

#include "usertypes.hpp"

#include <cstdint>

namespace host_test::fake_usart
{

void reset() noexcept;
void set_start_status(types::status status) noexcept;
void set_transmit_status(types::status status) noexcept;
std::uint32_t start_calls() noexcept;
std::uint32_t last_delivery() noexcept;

} // namespace host_test::fake_usart
