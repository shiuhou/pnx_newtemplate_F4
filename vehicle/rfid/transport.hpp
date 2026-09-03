#pragma once

#include <bsp_usart.hpp>

#include <cstddef>
#include <cstdint>

namespace vehicle::rfid::transport
{

bool open(bsp::usart::port selected, std::uint32_t baud,
          std::uint8_t* rx_buffer, std::size_t rx_buffer_size,
          bsp::usart::rx_handler handler, void* user_data) noexcept;
bool write(bsp::usart::port selected, const std::uint8_t* data,
           std::size_t size) noexcept;

} // namespace vehicle::rfid::transport
