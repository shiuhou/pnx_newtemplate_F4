#include "bsp_dwt.hpp"
#include "bsp_indicator.hpp"
#include "bsp_usart.hpp"
#include "cboard_demo_debug.hpp"
#include "config.hpp"
#include "tx_api.h"

#include <cstdio>
#include <cstdint>

namespace
{

TX_THREAD smoke_thread{};
alignas(8) ULONG smoke_stack[512]{};
CHAR smoke_name[] = "cboard smoke";
std::uint8_t telemetry_buffer[256]{};

std::uint32_t status_code(types::status status) noexcept
{
    return static_cast<std::uint32_t>(status);
}

void update_time() noexcept
{
    demo_debug_instance.threadx_tick =
        static_cast<std::uint32_t>(tx_time_get());
    cboard::demo::set_dwt_time(bsp::dwt::timeline_us());
}

void update_uart() noexcept
{
    const bsp::usart::telemetry stats =
        bsp::usart::snapshot(app::uart::usart1);
    demo_debug_instance.uart_tx_count = stats.tx_count;
    demo_debug_instance.uart_error_count = stats.error_count;
    demo_debug_instance.uart_busy_count = stats.busy_count;
}

void send_telemetry() noexcept
{
    const int length = std::snprintf(
        reinterpret_cast<char*>(telemetry_buffer), sizeof(telemetry_buffer),
        "PNX CBOARD SMOKE heartbeat=%lu tick=%lu dwt_us=%llu "
        "faults=%lu boot=%lu reset=0x%08lx crash=%lu "
        "stack_free=%lu pool_free=%lu\r\n",
        static_cast<unsigned long>(demo_debug_instance.heartbeat_count),
        static_cast<unsigned long>(demo_debug_instance.threadx_tick),
        static_cast<unsigned long long>(bsp::dwt::timeline_us()),
        static_cast<unsigned long>(demo_debug_instance.fault_count),
        static_cast<unsigned long>(demo_debug_instance.reset_count),
        static_cast<unsigned long>(
            demo_debug_instance.reset_reason_mask),
        static_cast<unsigned long>(demo_debug_instance.crash_valid),
        static_cast<unsigned long>(
            demo_debug_instance.thread_stack_free),
        static_cast<unsigned long>(
            demo_debug_instance.byte_pool_available));
    if (length <= 0)
    {
        ++demo_debug_instance.uart_error_count;
        return;
    }
    const std::size_t bytes =
        static_cast<std::size_t>(length) < sizeof(telemetry_buffer)
            ? static_cast<std::size_t>(length)
            : sizeof(telemetry_buffer) - 1U;
    const types::status status = bsp::usart::transmit(
        app::uart::usart1, telemetry_buffer, bytes, 20U);
    if (status != types::status::ok && status != types::status::busy)
    {
        demo_debug_instance.uart_status = status_code(status);
    }
}

void smoke_entry(ULONG)
{
    demo_debug_instance.threadx_started = 1U;
    cboard::demo::sync_system_diagnostics();

    const types::status dwt_status = bsp::dwt::init();
    const types::status indicator_status = bsp::indicator::init();
    const types::status uart_status =
        bsp::usart::init(app::uart::usart1, bsp::usart::mode::dma);
    demo_debug_instance.indicator_status = status_code(indicator_status);
    demo_debug_instance.uart_status = status_code(uart_status);

    if (dwt_status != types::status::ok ||
        indicator_status != types::status::ok ||
        uart_status != types::status::ok)
    {
        ++demo_debug_instance.fault_count;
        (void)bsp::indicator::set(bsp::indicator::channel::red, true);
    }

    for (;;)
    {
        ++demo_debug_instance.heartbeat_count;
        update_time();
        cboard::demo::sync_threadx(&smoke_thread);
        (void)bsp::indicator::toggle(bsp::indicator::channel::green);
        update_uart();
        send_telemetry();
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2U);
    }
}

} // namespace

extern "C" void app_start(void)
{
    demo_debug_instance.demo_kind =
        static_cast<std::uint32_t>(cboard::demo::kind::board_smoke);
    const UINT status =
        tx_thread_create(&smoke_thread, smoke_name, smoke_entry, 0U,
                         smoke_stack, sizeof(smoke_stack), 10U, 10U,
                         TX_NO_TIME_SLICE, TX_AUTO_START);
    demo_debug_instance.start_status = status;
    if (status != TX_SUCCESS)
    {
        ++demo_debug_instance.fault_count;
    }
}
