#include "bsp_usart.hpp"
#include "usart_support.hpp"

#include <array>
#include <cstdlib>

namespace host_test::fake_usart
{
void reset() noexcept;
void set_transmit_result(types::status result) noexcept;
void set_restart_result(types::status result) noexcept;
void receive(
    bsp::usart::port selected, std::size_t len) noexcept;
} // namespace host_test::fake_usart

namespace
{

std::size_t received_len = 0U;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

void on_rx(
    bsp::usart::port, const bsp::usart::rx_frame& frame,
    void*) noexcept
{
    received_len = frame.len;
}

} // namespace

int main()
{
    std::array<std::uint8_t, 8U> activation_rx{};
    board::usart::rx_activation_state activation{};
    bool metadata_visible_during_start = false;
    require(board::usart::activate_rx(
                activation, activation_rx.data(),
                activation_rx.size(),
                bsp::usart::rx_delivery::stream_segments,
                [&]() noexcept {
                    metadata_visible_during_start =
                        activation.buffer == activation_rx.data() &&
                        activation.len == activation_rx.size() &&
                        activation.cursor == 0U &&
                        activation.delivery ==
                            bsp::usart::rx_delivery::stream_segments;
                    return types::status::ok;
                }) == types::status::ok);
    require(metadata_visible_during_start);

    std::array<std::uint8_t, 4U> rejected_rx{};
    require(board::usart::activate_rx(
                activation, rejected_rx.data(), rejected_rx.size(),
                bsp::usart::rx_delivery::frame_snapshot,
                []() noexcept {
                    return types::status::error;
                }) == types::status::error);
    require(activation.buffer == activation_rx.data());
    require(activation.len == activation_rx.size());
    require(activation.delivery ==
            bsp::usart::rx_delivery::stream_segments);

    host_test::fake_usart::reset();

    require(bsp::usart::port_enabled(0U));
    const bsp::usart::line_config ps2_line{
        9600U,
        bsp::usart::word_length::bits_8,
        bsp::usart::stop_bits::one,
        bsp::usart::parity::none,
        true,
        true,
    };
    require(bsp::usart::configure(
                0U, ps2_line) == types::status::ok);
    auto invalid_line = ps2_line;
    invalid_line.baud_rate = 0U;
    require(bsp::usart::configure(
                0U, invalid_line) == types::status::invalid_arg);
    invalid_line = ps2_line;
    invalid_line.enable_tx = false;
    invalid_line.enable_rx = false;
    require(bsp::usart::configure(
                0U, invalid_line) == types::status::invalid_arg);
    require(bsp::usart::configure(
                3U, ps2_line) == types::status::not_configured);
    require(bsp::usart::configure(
                bsp::usart::port_count,
                ps2_line) == types::status::invalid_arg);
    require(bsp::usart::init(
                0U, bsp::usart::mode::dma) ==
            types::status::ok);
    require(bsp::usart::init(
                0U, bsp::usart::mode::block) ==
            types::status::invalid_arg);

    std::array<std::uint8_t, 18U> rx{};
    require(bsp::usart::start_rx_to_idle(
                0U, rx.data(), rx.size(), on_rx, nullptr) ==
            types::status::ok);
    host_test::fake_usart::receive(0U, rx.size());
    require(received_len == rx.size());
    const auto rx_stats = bsp::usart::snapshot(0U);
    require(rx_stats.rx_count == 1U);
    require(rx_stats.last_rx_len == rx.size());
    require(bsp::usart::restart_rx(0U) ==
            types::status::ok);

    host_test::fake_usart::set_restart_result(
        types::status::busy);
    require(bsp::usart::restart_rx(0U) ==
            types::status::busy);
    const auto restart_stats = bsp::usart::snapshot(0U);
    require(restart_stats.busy_count == 1U);
    require(restart_stats.error_count == 0U);

    require(bsp::usart::init(
                1U, bsp::usart::mode::block) ==
            types::status::ok);
    const std::uint8_t byte = 0x5AU;
    require(bsp::usart::transmit(
                1U, &byte, 1U, 10U) ==
            types::status::ok);
    require(bsp::usart::snapshot(1U).tx_count == 1U);

    require(bsp::usart::init(
                2U, bsp::usart::mode::dma) ==
            types::status::ok);
    host_test::fake_usart::set_transmit_result(
        types::status::busy);
    require(bsp::usart::transmit(
                2U, &byte, 1U, 10U) ==
            types::status::busy);
    require(bsp::usart::snapshot(2U).busy_count == 1U);

    require(bsp::usart::transmit(
                bsp::usart::port_count, &byte, 1U, 10U) ==
            types::status::invalid_arg);
    return EXIT_SUCCESS;
}
