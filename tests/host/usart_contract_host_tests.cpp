#include "bsp_usart.hpp"

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
    host_test::fake_usart::reset();

    require(bsp::usart::port_enabled(0U));
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
