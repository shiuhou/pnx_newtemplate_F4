#include "bsp_dwt.hpp"
#include "bsp_indicator.hpp"
#include "bsp_usart.hpp"
#include "cboard_demo_debug.hpp"
#include "config.hpp"
#include "dji_motor_service.hpp"
#include "tx_api.h"

#include <cstdio>
#include <cstdint>

namespace
{

TX_THREAD monitor_thread{};
alignas(8) ULONG monitor_stack[512]{};
CHAR monitor_name[] = "cboard can rx";
std::uint8_t telemetry_buffer[192]{};

std::uint32_t status_code(types::status status) noexcept
{
    return static_cast<std::uint32_t>(status);
}

void send_telemetry() noexcept
{
    const int length = std::snprintf(
        reinterpret_cast<char*>(telemetry_buffer),
        sizeof(telemetry_buffer),
        "PNX CBOARD CAN_RX frames=%lu id=0x%03lx tick=%lu "
        "errors=%lu busoff=%lu drop=%lu dji=%u online=%lu\r\n",
        static_cast<unsigned long>(demo_debug_instance.can_rx_count),
        static_cast<unsigned long>(demo_debug_instance.can_last_id),
        static_cast<unsigned long>(demo_debug_instance.can_last_tick),
        static_cast<unsigned long>(
            demo_debug_instance.can_error_count),
        static_cast<unsigned long>(
            demo_debug_instance.can_bus_off_count),
        static_cast<unsigned long>(demo_debug_instance.can_drop_count),
        static_cast<unsigned>(demo_debug_instance.dji_motor_id),
        static_cast<unsigned long>(demo_debug_instance.dji_online));
    if (length <= 0)
    {
        ++demo_debug_instance.uart_error_count;
        return;
    }
    const std::size_t bytes =
        static_cast<std::size_t>(length) < sizeof(telemetry_buffer)
            ? static_cast<std::size_t>(length)
            : sizeof(telemetry_buffer) - 1U;
    (void)bsp::usart::transmit(
        app::uart::usart1, telemetry_buffer, bytes, 20U);
}

void monitor_entry(ULONG)
{
    demo_debug_instance.threadx_started = 1U;
    cboard::demo::sync_system_diagnostics();

    const types::status dwt_status = bsp::dwt::init();
    const types::status indicator_status = bsp::indicator::init();
    const types::status uart_status =
        bsp::usart::init(app::uart::usart1, bsp::usart::mode::dma);
    const types::status service_status =
        modules::motor::dji_motor_service::instance().start(
            modules::motor::service_mode::receive_only,
            bsp::can::bus::can1);

    demo_debug_instance.indicator_status =
        status_code(indicator_status);
    demo_debug_instance.uart_status = status_code(uart_status);
    if (dwt_status != types::status::ok ||
        indicator_status != types::status::ok ||
        uart_status != types::status::ok ||
        service_status != types::status::ok)
    {
        ++demo_debug_instance.fault_count;
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
    }

    for (;;)
    {
        ++demo_debug_instance.heartbeat_count;
        demo_debug_instance.threadx_tick =
            static_cast<std::uint32_t>(tx_time_get());
        cboard::demo::set_dwt_time(bsp::dwt::timeline_us());
        cboard::demo::sync_threadx(&monitor_thread);
        cboard::demo::sync_can(bsp::can::bus::can1);
        cboard::demo::sync_motor(
            modules::motor::dji_motor_service::instance().snapshot());

        const bsp::usart::telemetry uart =
            bsp::usart::snapshot(app::uart::usart1);
        demo_debug_instance.uart_tx_count = uart.tx_count;
        demo_debug_instance.uart_error_count = uart.error_count;
        demo_debug_instance.uart_busy_count = uart.busy_count;
        (void)bsp::indicator::toggle(
            bsp::indicator::channel::blue);
        send_telemetry();
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2U);
    }
}

} // namespace

extern "C" void app_start(void)
{
    demo_debug_instance.demo_kind =
        static_cast<std::uint32_t>(
            cboard::demo::kind::can_receive);
    const UINT status = tx_thread_create(
        &monitor_thread, monitor_name, monitor_entry, 0U,
        monitor_stack, sizeof(monitor_stack), 10U, 10U,
        TX_NO_TIME_SLICE, TX_AUTO_START);
    demo_debug_instance.start_status = status;
    if (status != TX_SUCCESS)
    {
        ++demo_debug_instance.fault_count;
    }
}
