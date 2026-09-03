#include "vehicle/rfid/transport.hpp"

namespace vehicle::rfid::transport
{

bool open(bsp::usart::port selected, std::uint32_t baud,
          std::uint8_t* rx_buffer, std::size_t rx_buffer_size,
          bsp::usart::rx_handler handler, void* user_data) noexcept
{
    const bsp::usart::line_config line{
        baud,
        bsp::usart::word_length::bits_8,
        bsp::usart::stop_bits::one,
        bsp::usart::parity::none,
        true,
        true,
    };
    return bsp::usart::configure(selected, line) == types::status::ok &&
           bsp::usart::init(selected, bsp::usart::mode::dma) ==
               types::status::ok &&
           bsp::usart::start_rx_to_idle(
               selected, rx_buffer, rx_buffer_size, handler, user_data,
               nullptr, nullptr, bsp::usart::rx_delivery::stream_segments) ==
               types::status::ok;
}

bool write(bsp::usart::port selected, const std::uint8_t* data,
           std::size_t size) noexcept
{
    return bsp::usart::transmit(selected, data, size, 20U) ==
           types::status::ok;
}

} // namespace vehicle::rfid::transport
