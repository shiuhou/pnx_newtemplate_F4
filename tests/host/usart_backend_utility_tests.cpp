#include "bsp_usart.hpp"
#include "bsp_usart_backend.hpp"

#include <atomic>
#include <cstdlib>
#include <thread>
#include <vector>

namespace
{

[[noreturn]] void fail() noexcept
{
    std::abort();
}

void require(bool condition) noexcept
{
    if (!condition)
    {
        fail();
    }
}

} // namespace

int main()
{
    bsp::usart::detail::async_gate gate;
    std::atomic<std::uint32_t> winners{0U};
    std::vector<std::thread> workers;
    for (std::uint32_t i = 0U; i < 16U; ++i)
    {
        workers.emplace_back([&] {
            if (gate.try_acquire())
            {
                winners.fetch_add(1U, std::memory_order_relaxed);
            }
        });
    }
    for (auto& worker : workers)
    {
        worker.join();
    }
    require(winners.load(std::memory_order_relaxed) == 1U);
    gate.release();
    require(gate.try_acquire());
    gate.release();

    {
        bsp::usart::detail::async_gate_lease lease(gate);
        require(lease.acquired());
    }
    require(gate.try_acquire());
    gate.release();

    {
        bsp::usart::detail::async_gate_lease lease(gate);
        require(lease.acquired());
        lease.hand_off_to_isr();
    }
    require(!gate.try_acquire());
    gate.release();

    constexpr auto first =
        bsp::usart::detail::circular_rx_segments(
            0U, 4U, 8U,
            bsp::usart::detail::rx_event::half_transfer);
    static_assert(first.first.offset == 0U && first.first.len == 4U);
    static_assert(first.second.len == 0U && first.next == 4U);

    constexpr auto terminal =
        bsp::usart::detail::circular_rx_segments(
            4U, 8U, 8U,
            bsp::usart::detail::rx_event::transfer_complete);
    static_assert(terminal.first.offset == 4U &&
                  terminal.first.len == 4U);
    static_assert(terminal.next == 0U);

    constexpr auto wrapped =
        bsp::usart::detail::circular_rx_segments(
            6U, 2U, 8U, bsp::usart::detail::rx_event::idle);
    static_assert(wrapped.first.offset == 6U &&
                  wrapped.first.len == 2U);
    static_assert(wrapped.second.offset == 0U &&
                  wrapped.second.len == 2U);
    static_assert(wrapped.next == 2U);

    constexpr auto duplicate =
        bsp::usart::detail::circular_rx_segments(
            2U, 2U, 8U, bsp::usart::detail::rx_event::idle);
    static_assert(duplicate.first.len == 0U &&
                  duplicate.second.len == 0U);

    constexpr auto tc_then_idle =
        bsp::usart::detail::circular_rx_segments(
            terminal.next, 8U, 8U,
            bsp::usart::detail::rx_event::idle);
    static_assert(tc_then_idle.first.len == 0U &&
                  tc_then_idle.second.len == 0U);

    constexpr auto idle_at_end_without_tc =
        bsp::usart::detail::circular_rx_segments(
            4U, 8U, 8U, bsp::usart::detail::rx_event::idle);
    static_assert(idle_at_end_without_tc.first.offset == 4U &&
                  idle_at_end_without_tc.first.len == 4U &&
                  idle_at_end_without_tc.next == 0U);

    static_assert(
        sizeof(bsp::usart::dma_rx_storage<18U>) == 32U);
    static_assert(
        alignof(bsp::usart::dma_rx_storage<18U>) ==
        bsp::usart::dma_cache_line_size);

    static_assert(
        !bsp::usart::detail::async_tx_error_releases_gate(
            false, false, false, false));
    static_assert(
        !bsp::usart::detail::async_tx_error_releases_gate(
            true, true, false, true));
    static_assert(
        !bsp::usart::detail::async_tx_error_releases_gate(
            true, false, true, false));
    static_assert(
        bsp::usart::detail::async_tx_error_releases_gate(
            true, false, false, false));
    // STM32 HAL leaves CR3.DMAT set when UART_DMAError ends a TX transfer.
    // A latched HAL_UART_ERROR_DMA plus READY TX state is therefore terminal.
    static_assert(
        bsp::usart::detail::async_tx_error_releases_gate(
            true, false, true, true));
    return 0;
}
