#include "bsp_dwt.hpp"
#include "bsp_indicator.hpp"
#include "bsp_usart.hpp"
#include "bsp_usb.hpp"
#include "cboard_demo_debug.hpp"
#include "config.hpp"
#include "tx_api.h"

#include <cstdio>
#include <cstdint>

#ifndef PNX_USB_DEVICE_IDENTITY_CONFIRMED
#define PNX_USB_DEVICE_IDENTITY_CONFIRMED 0
#endif

namespace
{

TX_THREAD usb_demo_thread{};
alignas(8) ULONG usb_demo_stack[512]{};
CHAR usb_demo_name[] = "cboard usb cdc";
std::uint8_t usb_telemetry_buffer[160]{};
std::uint8_t uart_telemetry_buffer[256]{};
bool uart_usb_line_next = false;

std::uint32_t status_code(types::status status) noexcept
{
    return static_cast<std::uint32_t>(status);
}

[[maybe_unused]] void echo_received(
    const std::uint8_t* data, std::uint16_t length, void*) noexcept
{
    if (data == nullptr || length == 0U)
    {
        return;
    }
    const bsp::usb::write_result result = bsp::usb::write(data, length);
    if (result.status != types::status::ok)
    {
        ++demo_debug_instance.fault_count;
    }
}

void update_uart() noexcept
{
    const bsp::usart::telemetry stats =
        bsp::usart::snapshot(app::uart::usart1);
    demo_debug_instance.uart_tx_count = stats.tx_count;
    demo_debug_instance.uart_error_count = stats.error_count;
    demo_debug_instance.uart_busy_count = stats.busy_count;
}

void send_usb_telemetry() noexcept
{
    if (!bsp::usb::connected())
    {
        return;
    }
    const int length = std::snprintf(
        reinterpret_cast<char*>(usb_telemetry_buffer),
        sizeof(usb_telemetry_buffer),
        "PNX_F407_USB_CDC READY heartbeat=%lu rx=%lu tx=%lu "
        "errors=%lu drops=%lu\r\n",
        static_cast<unsigned long>(
            demo_debug_instance.heartbeat_count),
        static_cast<unsigned long>(
            demo_debug_instance.usb_read_count),
        static_cast<unsigned long>(
            demo_debug_instance.usb_write_count),
        static_cast<unsigned long>(
            demo_debug_instance.usb_error_count),
        static_cast<unsigned long>(
            demo_debug_instance.usb_tx_drop_count));
    if (length <= 0)
    {
        ++demo_debug_instance.fault_count;
        return;
    }
    const std::size_t bytes =
        static_cast<std::size_t>(length) < sizeof(usb_telemetry_buffer)
            ? static_cast<std::size_t>(length)
            : sizeof(usb_telemetry_buffer) - 1U;
    const bsp::usb::write_result result =
        bsp::usb::write(usb_telemetry_buffer, bytes);
    if (result.status != types::status::ok &&
        result.status != types::status::busy &&
        result.status != types::status::not_configured)
    {
        ++demo_debug_instance.fault_count;
    }
}

void send_uart_telemetry() noexcept
{
    const bsp::usb::runtime_state usb_state = bsp::usb::snapshot();
    const char* identity =
        demo_debug_instance.usb_identity_confirmed != 0U
            ? "confirmed"
            : "blocked-unassigned";
    int length = 0;
    if (uart_usb_line_next)
    {
        length = std::snprintf(
            reinterpret_cast<char*>(uart_telemetry_buffer),
            sizeof(uart_telemetry_buffer),
            "PNX_F407_USB usb identity=%s status=%lu init=%lu conn=%lu "
            "link=%lu capq=%lu pend=%u q=%u qhi=%u "
            "connect=%lu disconnect=%lu err=%lu drop=%lu\r\n",
            identity,
            static_cast<unsigned long>(demo_debug_instance.usb_status),
            static_cast<unsigned long>(
                demo_debug_instance.usb_initialized),
            static_cast<unsigned long>(
                demo_debug_instance.usb_connected),
            static_cast<unsigned long>(
                demo_debug_instance.usb_link_state),
            static_cast<unsigned long>(
                demo_debug_instance.usb_tx_queue_depth),
            static_cast<unsigned int>(usb_state.pending_write_len),
            static_cast<unsigned int>(usb_state.tx_queue_size),
            static_cast<unsigned int>(usb_state.tx_queue_high_water),
            static_cast<unsigned long>(
                demo_debug_instance.usb_connect_count),
            static_cast<unsigned long>(
                demo_debug_instance.usb_disconnect_count),
            static_cast<unsigned long>(
                demo_debug_instance.usb_error_count),
            static_cast<unsigned long>(
                demo_debug_instance.usb_tx_drop_count));
    }
    else
    {
        length = std::snprintf(
            reinterpret_cast<char*>(uart_telemetry_buffer),
            sizeof(uart_telemetry_buffer),
            "PNX_F407_USB core abi=%lu hb=%lu tick=%lu dwt_us=%llu "
            "start=%lu thread=%lu stack_free=%lu pool_free=%lu "
            "fault=%lu reset=0x%08lx crash=%lu\r\n",
            static_cast<unsigned long>(demo_debug_instance.abi_version),
            static_cast<unsigned long>(
                demo_debug_instance.heartbeat_count),
            static_cast<unsigned long>(demo_debug_instance.threadx_tick),
            static_cast<unsigned long long>(bsp::dwt::timeline_us()),
            static_cast<unsigned long>(demo_debug_instance.start_status),
            static_cast<unsigned long>(
                demo_debug_instance.thread_info_status),
            static_cast<unsigned long>(
                demo_debug_instance.thread_stack_free),
            static_cast<unsigned long>(
                demo_debug_instance.byte_pool_available),
            static_cast<unsigned long>(demo_debug_instance.fault_count),
            static_cast<unsigned long>(
                demo_debug_instance.reset_reason_mask),
            static_cast<unsigned long>(demo_debug_instance.crash_valid));
    }

    if (length <= 0)
    {
        ++demo_debug_instance.fault_count;
        return;
    }
    const std::size_t bytes =
        static_cast<std::size_t>(length) < sizeof(uart_telemetry_buffer)
            ? static_cast<std::size_t>(length)
            : sizeof(uart_telemetry_buffer) - 1U;
    const types::status status = bsp::usart::transmit(
        app::uart::usart1, uart_telemetry_buffer, bytes, 20U);
    if (status == types::status::ok)
    {
        uart_usb_line_next = !uart_usb_line_next;
    }
    else if (status != types::status::busy)
    {
        demo_debug_instance.uart_status = status_code(status);
        ++demo_debug_instance.fault_count;
    }
}

void usb_demo_entry(ULONG)
{
    demo_debug_instance.threadx_started = 1U;
    demo_debug_instance.usb_identity_confirmed =
        PNX_USB_DEVICE_IDENTITY_CONFIRMED ? 1U : 0U;
    cboard::demo::sync_system_diagnostics();

    const types::status dwt_status = bsp::dwt::init();
    const types::status indicator_status = bsp::indicator::init();
    const types::status uart_status =
        bsp::usart::init(app::uart::usart1, bsp::usart::mode::dma);
    types::status usb_status = types::status::not_configured;
#if PNX_USB_DEVICE_IDENTITY_CONFIRMED
    bsp::usb::config usb_config{};
    usb_config.write_priority = 10U;
    usb_config.min_rx_size = 1U;
    usb_config.max_tx_size = 64U;
    usb_config.on_rx = echo_received;
    usb_status = bsp::usb::init(usb_config);
#endif
    demo_debug_instance.indicator_status =
        status_code(indicator_status);
    demo_debug_instance.uart_status = status_code(uart_status);
    demo_debug_instance.usb_status = status_code(usb_status);

    const bool usb_status_expected =
#if PNX_USB_DEVICE_IDENTITY_CONFIRMED
        usb_status == types::status::ok;
#else
        usb_status == types::status::not_configured;
#endif
    if (dwt_status != types::status::ok ||
        indicator_status != types::status::ok ||
        uart_status != types::status::ok ||
        !usb_status_expected)
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
        cboard::demo::sync_threadx(&usb_demo_thread);
        cboard::demo::sync_usb();
        update_uart();
        (void)bsp::indicator::toggle(
            bsp::indicator::channel::green);
        send_uart_telemetry();
        send_usb_telemetry();
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2U);
    }
}

} // namespace

extern "C" void app_start(void)
{
    demo_debug_instance.demo_kind =
        static_cast<std::uint32_t>(cboard::demo::kind::usb_cdc);
    const UINT status = tx_thread_create(
        &usb_demo_thread, usb_demo_name, usb_demo_entry, 0U,
        usb_demo_stack, sizeof(usb_demo_stack), 10U, 10U,
        TX_NO_TIME_SLICE, TX_AUTO_START);
    demo_debug_instance.start_status = status;
    if (status != TX_SUCCESS)
    {
        ++demo_debug_instance.fault_count;
    }
}
