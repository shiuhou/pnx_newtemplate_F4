#include "vehicle/rfid/protocol.hpp"
#include "vehicle/rfid/transport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace host_test::fake_usart
{
void reset() noexcept;
void receive_bytes(bsp::usart::port selected, const std::uint8_t* bytes,
                   std::size_t len) noexcept;
bsp::usart::line_config configured_line(
    bsp::usart::port selected) noexcept;
bsp::usart::rx_delivery configured_delivery(
    bsp::usart::port selected) noexcept;
std::size_t copy_last_transmit(bsp::usart::port selected,
                               std::uint8_t* output,
                               std::size_t capacity) noexcept;
} // namespace host_test::fake_usart

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

std::array<std::uint8_t, 16U> received{};
std::size_t received_size{};

void on_rx(bsp::usart::port selected,
           const bsp::usart::rx_frame& frame, void*) noexcept
{
    require(selected == app::uart::rfid);
    require(frame.len <= received.size());
    for (std::size_t i = 0U; i < frame.len; ++i)
    {
        received[i] = frame.data[i];
    }
    received_size = frame.len;
}

} // namespace

int main()
{
    host_test::fake_usart::reset();
    bsp::usart::dma_rx_storage<64U> dma_rx{};
    require(vehicle::rfid::transport::open(
        app::uart::rfid, params::rfid::baud, dma_rx.data(),
        dma_rx.logical_size, on_rx, nullptr));

    const auto line =
        host_test::fake_usart::configured_line(app::uart::rfid);
    require(line.baud_rate == 9600U);
    require(line.data_bits == bsp::usart::word_length::bits_8);
    require(line.stop == bsp::usart::stop_bits::one);
    require(line.parity_mode == bsp::usart::parity::none);
    require(line.enable_tx && line.enable_rx);
    require(host_test::fake_usart::configured_delivery(app::uart::rfid) ==
            bsp::usart::rx_delivery::stream_segments);

    constexpr std::array<std::uint8_t, 4U> incoming{
        0x04U, 0x0CU, 0x02U, 0x20U};
    host_test::fake_usart::receive_bytes(
        app::uart::rfid, incoming.data(), incoming.size());
    require(received_size == incoming.size());
    require(received[0] == incoming[0] && received[3] == incoming[3]);

    const auto query = vehicle::rfid::protocol::make_query(
        vehicle::rfid::protocol::command::b1,
        static_cast<std::uint8_t>(params::rfid::address));
    require(vehicle::rfid::transport::write(
        app::uart::rfid, query.data(), query.size()));
    std::array<std::uint8_t, 8U> transmitted{};
    require(host_test::fake_usart::copy_last_transmit(
                app::uart::rfid, transmitted.data(), transmitted.size()) ==
            transmitted.size());
    require(transmitted == query);
    return EXIT_SUCCESS;
}
