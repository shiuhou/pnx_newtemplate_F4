#include "bsp_usart.hpp"
#include "fake_usart_backend.hpp"

#include <atomic>

namespace
{

std::atomic<types::status> start_status{types::status::ok};
std::atomic<types::status> transmit_status{types::status::ok};
std::atomic<std::uint32_t> start_call_count{0U};
std::atomic<std::uint32_t> last_start_delivery{0U};

} // namespace

namespace host_test::fake_usart
{

void reset() noexcept
{
    start_status.store(types::status::ok, std::memory_order_relaxed);
    transmit_status.store(types::status::ok, std::memory_order_relaxed);
    start_call_count.store(0U, std::memory_order_relaxed);
    last_start_delivery.store(0U, std::memory_order_relaxed);
}

void set_start_status(types::status status) noexcept
{
    start_status.store(status, std::memory_order_relaxed);
}

void set_transmit_status(types::status status) noexcept
{
    transmit_status.store(status, std::memory_order_relaxed);
}

std::uint32_t start_calls() noexcept
{
    return start_call_count.load(std::memory_order_relaxed);
}

std::uint32_t last_delivery() noexcept
{
    return last_start_delivery.load(std::memory_order_relaxed);
}

} // namespace host_test::fake_usart

namespace bsp::usart::detail
{

types::status backend_init(port, mode) noexcept
{
    return types::status::ok;
}

types::status backend_transmit(port, mode, const std::uint8_t*, std::size_t,
                               std::uint32_t) noexcept
{
    return transmit_status.load(std::memory_order_relaxed);
}

types::status backend_start_rx(port, mode, std::uint8_t*,
                               std::size_t,
                               rx_delivery delivery) noexcept
{
    start_call_count.fetch_add(1U, std::memory_order_relaxed);
    last_start_delivery.store(
        static_cast<std::uint32_t>(delivery),
        std::memory_order_relaxed);
    return start_status.load(std::memory_order_relaxed);
}

types::status backend_restart_rx(port) noexcept
{
    return types::status::ok;
}

} // namespace bsp::usart::detail
