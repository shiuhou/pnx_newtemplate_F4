#include "bsp_dwt.hpp"
#include "bsp_indicator.hpp"
#include "bsp_usart.hpp"
#include "bsp_usb.hpp"
#include "cboard_demo_debug.hpp"
#include "config.hpp"
#include "dji_motor_service.hpp"
#include "tx_api.h"
#include "usb_arm_command.hpp"

#include <atomic>
#include <cstdio>
#include <cstdint>

#ifndef PNX_MOTOR_TEST_USB_ARM
#define PNX_MOTOR_TEST_USB_ARM 0
#endif

#ifndef PNX_USB_DEVICE_IDENTITY_CONFIRMED
#define PNX_USB_DEVICE_IDENTITY_CONFIRMED 0
#endif

namespace
{

TX_THREAD monitor_thread{};
alignas(8) ULONG monitor_stack[512]{};
CHAR monitor_name[] = "cboard motor safe";
std::uint8_t telemetry_buffer[208]{};
#if PNX_MOTOR_TEST_USB_ARM
std::uint8_t usb_telemetry_buffer[64]{};
std::atomic<bool> usb_arm_pending{false};
cboard::motor_safe::usb_arm_command_parser usb_arm_parser{};
std::uint8_t usb_motor_line_index = 0U;
#endif

constexpr bool nonzero_compiled =
    PNX_NONZERO_MOTOR_TEST_ENABLED != 0;
constexpr bsp::can::bus configured_bus =
    PNX_MOTOR_TEST_BUS_INDEX >= 0
        ? static_cast<bsp::can::bus>(PNX_MOTOR_TEST_BUS_INDEX)
        : bsp::can::bus::none;
constexpr bsp::can::bus startup_bus =
    nonzero_compiled ? configured_bus : bsp::can::bus::can1;

std::uint32_t status_code(types::status status) noexcept
{
    return static_cast<std::uint32_t>(status);
}

modules::motor::nonzero_test_config test_config() noexcept
{
    modules::motor::nonzero_test_config config{};
    config.compiled_enabled = nonzero_compiled;
    config.selected_bus = configured_bus;
    config.feedback_id = PNX_MOTOR_TEST_ID;
    config.requested_current = PNX_MOTOR_TEST_CURRENT;
    config.device_model =
        static_cast<motors::dji::protocol::model>(
            PNX_MOTOR_TEST_MODEL);
    return config;
}

void usb_arm_received(
    const std::uint8_t* data, std::uint16_t length, void*) noexcept
{
#if PNX_MOTOR_TEST_USB_ARM
    if (usb_arm_parser.consume(data, length))
    {
        usb_arm_pending.store(true, std::memory_order_release);
    }
#else
    (void)data;
    (void)length;
#endif
}

void send_usb_telemetry() noexcept
{
#if PNX_MOTOR_TEST_USB_ARM
    const bsp::usb::runtime_state usb_state = bsp::usb::snapshot();
    if (!usb_state.connected || usb_state.write_busy ||
        usb_state.tx_queue_size != 0U)
    {
        return;
    }

    int length = 0;
    if (usb_motor_line_index == 0U)
    {
        length = std::snprintf(
            reinterpret_cast<char*>(usb_telemetry_buffer),
            sizeof(usb_telemetry_buffer),
            "MOTOR s=%08lx l=%08lx e=%08lx d=%08lx f=%08lx\r\n",
            static_cast<unsigned long>(demo_debug_instance.motor_arm_state),
            static_cast<unsigned long>(demo_debug_instance.motor_pulse_latched),
            static_cast<unsigned long>(demo_debug_instance.can_error_count),
            static_cast<unsigned long>(demo_debug_instance.can_drop_count),
            static_cast<unsigned long>(demo_debug_instance.motor_service_faults));
    }
    else if (usb_motor_line_index == 1U)
    {
        length = std::snprintf(
            reinterpret_cast<char*>(usb_telemetry_buffer),
            sizeof(usb_telemetry_buffer),
            "MOTOR z=%08lx n=%08lx r=%04x a=%04x\r\n",
            static_cast<unsigned long>(demo_debug_instance.motor_zero_tx_count),
            static_cast<unsigned long>(demo_debug_instance.motor_nonzero_tx_count),
            static_cast<unsigned int>(
                static_cast<std::uint16_t>(
                    demo_debug_instance.motor_requested_current)),
            static_cast<unsigned int>(
                static_cast<std::uint16_t>(
                    demo_debug_instance.motor_applied_current)));
    }
    else
    {
        length = std::snprintf(
            reinterpret_cast<char*>(usb_telemetry_buffer),
            sizeof(usb_telemetry_buffer),
            "MOTOR v=%04x c=%04x q=%08lx\r\n",
            static_cast<unsigned int>(
                static_cast<std::uint16_t>(
                    demo_debug_instance.dji_speed_rpm)),
            static_cast<unsigned int>(
                static_cast<std::uint16_t>(
                    demo_debug_instance.dji_current)),
            static_cast<unsigned long>(
                demo_debug_instance.dji_feedback_count));
    }
    if (length <= 0 ||
        static_cast<std::size_t>(length) >= sizeof(usb_telemetry_buffer))
    {
        ++demo_debug_instance.fault_count;
        return;
    }

    const bsp::usb::write_result result = bsp::usb::write(
        usb_telemetry_buffer, static_cast<std::size_t>(length));
    if (result.status == types::status::ok)
    {
        usb_motor_line_index =
            static_cast<std::uint8_t>((usb_motor_line_index + 1U) % 3U);
    }
    else if (result.status != types::status::busy &&
             result.status != types::status::not_configured)
    {
        ++demo_debug_instance.fault_count;
    }
#endif
}

void send_telemetry() noexcept
{
    const int length = std::snprintf(
        reinterpret_cast<char*>(telemetry_buffer),
        sizeof(telemetry_buffer),
        "PNX CBOARD MOTOR_SAFE rx=%lu id=0x%03lx online=%lu "
        "requested=%d applied=%d zero_tx=%lu nonzero_tx=%lu "
        "arm=%lu latched=%lu faults=%lu\r\n",
        static_cast<unsigned long>(
            demo_debug_instance.dji_feedback_count),
        static_cast<unsigned long>(demo_debug_instance.can_last_id),
        static_cast<unsigned long>(demo_debug_instance.dji_online),
        static_cast<int>(
            demo_debug_instance.motor_requested_current),
        static_cast<int>(
            demo_debug_instance.motor_applied_current),
        static_cast<unsigned long>(
            demo_debug_instance.motor_zero_tx_count),
        static_cast<unsigned long>(
            demo_debug_instance.motor_nonzero_tx_count),
        static_cast<unsigned long>(
            demo_debug_instance.motor_arm_state),
        static_cast<unsigned long>(
            demo_debug_instance.motor_pulse_latched),
        static_cast<unsigned long>(
            demo_debug_instance.motor_service_faults));
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
    demo_debug_instance.usb_identity_confirmed =
        PNX_USB_DEVICE_IDENTITY_CONFIRMED ? 1U : 0U;
    cboard::demo::sync_system_diagnostics();

    const types::status dwt_status = bsp::dwt::init();
    const types::status indicator_status = bsp::indicator::init();
    const types::status uart_status =
        bsp::usart::init(app::uart::usart1, bsp::usart::mode::dma);
    types::status usb_status = types::status::not_configured;
#if PNX_MOTOR_TEST_USB_ARM && PNX_USB_DEVICE_IDENTITY_CONFIRMED
    bsp::usb::config usb_config{};
    usb_config.write_priority = 10U;
    usb_config.min_rx_size = 1U;
    usb_config.max_tx_size = 64U;
    usb_config.on_rx = usb_arm_received;
    usb_status = bsp::usb::init(usb_config);
#endif
    auto& service =
        modules::motor::dji_motor_service::instance();
    const modules::motor::nonzero_test_config config =
        test_config();
    demo_debug_instance.motor_requested_current =
        config.requested_current;
    const types::status config_status =
        service.configure_nonzero_test(config);
    const types::status service_status =
        config_status == types::status::ok
            ? service.start(
                  modules::motor::service_mode::safe_command,
                  startup_bus)
            : config_status;

    demo_debug_instance.indicator_status =
        status_code(indicator_status);
    demo_debug_instance.uart_status = status_code(uart_status);
    demo_debug_instance.usb_status = status_code(usb_status);
    const bool usb_status_expected =
#if PNX_MOTOR_TEST_USB_ARM
        usb_status == types::status::ok;
#else
        usb_status == types::status::not_configured;
#endif
    if (dwt_status != types::status::ok ||
        indicator_status != types::status::ok ||
        uart_status != types::status::ok ||
        !usb_status_expected ||
        config_status != types::status::ok ||
        service_status != types::status::ok)
    {
        ++demo_debug_instance.fault_count;
        service.force_zero();
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
    }

    for (;;)
    {
#if PNX_MOTOR_TEST_USB_ARM
        if (usb_arm_pending.exchange(false, std::memory_order_acquire))
        {
            if (service.request_arm(modules::motor::motor_demo_arm_magic) !=
                types::status::ok)
            {
                ++demo_debug_instance.fault_count;
            }
        }
#else
        const std::uint32_t token = pnx_motor_demo_arm_token;
        if (token != 0U)
        {
            pnx_motor_demo_arm_token = 0U;
            if (service.request_arm(token) != types::status::ok)
            {
                ++demo_debug_instance.fault_count;
            }
        }
#endif

        ++demo_debug_instance.heartbeat_count;
        demo_debug_instance.threadx_tick =
            static_cast<std::uint32_t>(tx_time_get());
        cboard::demo::set_dwt_time(bsp::dwt::timeline_us());
        cboard::demo::sync_threadx(&monitor_thread);
        cboard::demo::sync_can(startup_bus);
        const modules::motor::service_telemetry service_state =
            service.snapshot();
        cboard::demo::sync_motor(service_state);
#if PNX_MOTOR_TEST_USB_ARM
        cboard::demo::sync_usb();
#endif

        const bsp::usart::telemetry uart =
            bsp::usart::snapshot(app::uart::usart1);
        demo_debug_instance.uart_tx_count = uart.tx_count;
        demo_debug_instance.uart_error_count = uart.error_count;
        demo_debug_instance.uart_busy_count = uart.busy_count;

        if (service_state.fault_latched)
        {
            (void)bsp::indicator::set(
                bsp::indicator::channel::red, true);
        }
        else
        {
            (void)bsp::indicator::toggle(
                bsp::indicator::channel::green);
        }
        send_telemetry();
        if ((demo_debug_instance.heartbeat_count % 25U) == 0U)
        {
            send_usb_telemetry();
        }
        tx_thread_sleep(20U);
    }
}

} // namespace

extern "C" void app_start(void)
{
    demo_debug_instance.demo_kind =
        static_cast<std::uint32_t>(
            cboard::demo::kind::motor_safe);
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
