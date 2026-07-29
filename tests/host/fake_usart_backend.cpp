#include "bsp_usart.hpp"

#include <array>

namespace
{

std::array<std::uint8_t*, bsp::usart::port_count> rx_buffers{};
std::array<std::size_t, bsp::usart::port_count> rx_lengths{};
types::status transmit_result = types::status::ok;
types::status restart_result = types::status::ok;

} // namespace

namespace host_test::fake_usart
{

void reset() noexcept
{
    rx_buffers = {};
    rx_lengths = {};
    transmit_result = types::status::ok;
    restart_result = types::status::ok;
}

void set_transmit_result(types::status result) noexcept
{
    transmit_result = result;
}

void set_restart_result(types::status result) noexcept
{
    restart_result = result;
}

void receive(
    bsp::usart::port selected, std::size_t len) noexcept
{
    if (selected < bsp::usart::port_count &&
        rx_buffers[selected] != nullptr &&
        len <= rx_lengths[selected])
    {
        bsp::usart::detail::rx_from_isr(selected, len);
    }
}

} // namespace host_test::fake_usart

namespace bsp::usart::detail
{

types::status backend_init(port, mode) noexcept
{
    return types::status::ok;
}

types::status backend_transmit(
    port, mode selected_mode, const std::uint8_t*,
    std::size_t, std::uint32_t) noexcept
{
    const types::status result = transmit_result;
    if (result == types::status::ok &&
        selected_mode != mode::block)
    {
        tx_complete_from_isr(0U);
    }
    return result;
}

types::status backend_start_rx(
    port selected, mode selected_mode,
    std::uint8_t* buffer, std::size_t len,
    rx_delivery) noexcept
{
    if (selected_mode != mode::dma)
    {
        return types::status::invalid_arg;
    }
    rx_buffers[selected] = buffer;
    rx_lengths[selected] = len;
    return types::status::ok;
}

types::status backend_restart_rx(port selected) noexcept
{
    if (selected >= port_count || rx_buffers[selected] == nullptr)
    {
        return types::status::invalid_arg;
    }
    return restart_result;
}

} // namespace bsp::usart::detail
