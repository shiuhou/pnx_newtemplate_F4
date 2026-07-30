#pragma once

#include <cstddef>
#include <cstdint>

namespace host_test::fake_spi
{

void reset() noexcept;
bool initialized() noexcept;
bool selected(std::uint8_t line) noexcept;
std::size_t transmit_count() noexcept;
std::size_t receive_count() noexcept;

} // namespace host_test::fake_spi
