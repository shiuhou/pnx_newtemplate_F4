#pragma once

#include "bsp_usb.hpp"

#include <array>
#include <cstdint>

namespace host_test::fake_usb
{

void reset() noexcept;
void set_init_status(types::status status) noexcept;
std::uint32_t init_calls() noexcept;
std::uint32_t signal_calls() noexcept;
void set_connected(bool connected) noexcept;
bool pop_tx(std::array<std::uint8_t, 128>& output,
            std::uint16_t& length) noexcept;
void complete_tx(std::uint16_t requested, std::uint16_t actual,
                 types::status status,
                 std::uint32_t native_status) noexcept;
void inject_rx(const std::uint8_t* data, std::uint16_t length,
               types::status status = types::status::ok,
               std::uint32_t native_status = 0U) noexcept;

} // namespace host_test::fake_usb
