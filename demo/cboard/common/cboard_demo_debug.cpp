#include "cboard_demo_debug.hpp"

#include "app_azure_rtos.h"
#include "bsp_diagnostics.h"
#include "cboard_threadx_metrics.hpp"
#if PNX_F407_USB_ENABLED
#include "bsp_usb.hpp"
#endif

#include <cstdint>

extern "C" {
volatile cboard::demo::debug_state demo_debug_instance{};
volatile std::uint32_t pnx_motor_demo_arm_token = 0U;
}

namespace cboard::demo
{

void set_dwt_time(std::uint64_t microseconds) noexcept
{
    demo_debug_instance.dwt_us_low =
        static_cast<std::uint32_t>(microseconds);
    demo_debug_instance.dwt_us_high =
        static_cast<std::uint32_t>(microseconds >> 32U);
}

void sync_system_diagnostics() noexcept
{
    bsp_diagnostics_snapshot snapshot{};
    bsp_diagnostics_get_snapshot(&snapshot);
    demo_debug_instance.reset_count = snapshot.boot_count;
    demo_debug_instance.reset_flags_raw = snapshot.reset_flags_raw;
    demo_debug_instance.reset_reason_mask =
        snapshot.reset_reason_mask;
    demo_debug_instance.crash_valid = snapshot.crash_valid;
    demo_debug_instance.crash_from_previous_boot =
        snapshot.crash_from_previous_boot;
    demo_debug_instance.crash_sequence = 0U;
    demo_debug_instance.crash_kind = 0U;
    demo_debug_instance.crash_pc = 0U;
    demo_debug_instance.crash_lr = 0U;
    demo_debug_instance.crash_cfsr = 0U;
    demo_debug_instance.crash_hfsr = 0U;
    demo_debug_instance.crash_mmfar = 0U;
    demo_debug_instance.crash_bfar = 0U;
    demo_debug_instance.crash_context = 0U;
    if (snapshot.crash_valid != 0U)
    {
        demo_debug_instance.crash_sequence = snapshot.crash.sequence;
        demo_debug_instance.crash_kind = snapshot.crash.kind;
        demo_debug_instance.crash_pc = snapshot.crash.pc;
        demo_debug_instance.crash_lr = snapshot.crash.lr;
        demo_debug_instance.crash_cfsr = snapshot.crash.cfsr;
        demo_debug_instance.crash_hfsr = snapshot.crash.hfsr;
        demo_debug_instance.crash_mmfar = snapshot.crash.mmfar;
        demo_debug_instance.crash_bfar = snapshot.crash.bfar;
        demo_debug_instance.crash_context = snapshot.crash.context;
    }
}

void sync_threadx(TX_THREAD* thread) noexcept
{
    sync_system_diagnostics();
    demo_debug_instance.thread_state = 0U;
    demo_debug_instance.thread_run_count = 0U;
    demo_debug_instance.thread_stack_size = 0U;
    demo_debug_instance.thread_stack_used = 0U;
    demo_debug_instance.thread_stack_free = 0U;
    if (thread == nullptr)
    {
        demo_debug_instance.thread_info_status = TX_PTR_ERROR;
    }
    else
    {
        UINT state = 0U;
        ULONG run_count = 0U;
        const UINT thread_status =
            tx_thread_info_get(thread, nullptr, &state, &run_count,
                               nullptr, nullptr, nullptr, nullptr,
                               nullptr);
        demo_debug_instance.thread_info_status = thread_status;
        if (thread_status == TX_SUCCESS)
        {
            demo_debug_instance.thread_state = state;
            demo_debug_instance.thread_run_count =
                static_cast<std::uint32_t>(run_count);
            const auto stack = threadx::stack_usage(
                reinterpret_cast<std::uintptr_t>(
                    thread->tx_thread_stack_start),
                reinterpret_cast<std::uintptr_t>(
                    thread->tx_thread_stack_end),
                reinterpret_cast<std::uintptr_t>(
                    thread->tx_thread_stack_highest_ptr));
            if (stack.valid)
            {
                demo_debug_instance.thread_stack_size =
                    static_cast<std::uint32_t>(stack.size_bytes);
                demo_debug_instance.thread_stack_used =
                    static_cast<std::uint32_t>(stack.used_bytes);
                demo_debug_instance.thread_stack_free =
                    static_cast<std::uint32_t>(stack.free_bytes);
            }
        }
    }

    demo_debug_instance.byte_pool_available = 0U;
    demo_debug_instance.byte_pool_fragments = 0U;
    ULONG available = 0U;
    ULONG fragments = 0U;
    const UINT pool_status = tx_byte_pool_info_get(
        pnx_threadx_app_pool(), nullptr, &available, &fragments,
        nullptr, nullptr, nullptr);
    demo_debug_instance.byte_pool_status = pool_status;
    if (pool_status == TX_SUCCESS)
    {
        demo_debug_instance.byte_pool_available =
            static_cast<std::uint32_t>(available);
        demo_debug_instance.byte_pool_fragments =
            static_cast<std::uint32_t>(fragments);
    }
}

void sync_can(bsp::can::bus bus) noexcept
{
    const bsp::can::telemetry telemetry = bsp::can::snapshot(bus);
    demo_debug_instance.can_rx_count = telemetry.rx_count;
    demo_debug_instance.can_last_id = telemetry.last_id;
    demo_debug_instance.can_last_tick = telemetry.last_tick;
    demo_debug_instance.can_error_count = telemetry.error_count;
    demo_debug_instance.can_bus_off_count = telemetry.bus_off_count;
    demo_debug_instance.can_drop_count = telemetry.drop_count;
    demo_debug_instance.can_bus_state =
        static_cast<std::uint32_t>(telemetry.bus_state);
}

void sync_motor(
    const modules::motor::service_telemetry& telemetry) noexcept
{
    demo_debug_instance.motor_service_faults =
        telemetry.service_fault_count;
    demo_debug_instance.motor_queue_overflow =
        telemetry.queue_overflow_count;
    demo_debug_instance.motor_zero_tx_count =
        telemetry.zero_tx_count;
    demo_debug_instance.motor_nonzero_tx_count =
        telemetry.nonzero_tx_count;
    demo_debug_instance.motor_applied_current =
        telemetry.applied_current;
    demo_debug_instance.motor_arm_state =
        static_cast<std::uint32_t>(telemetry.arm);
    demo_debug_instance.motor_pulse_latched =
        telemetry.pulse_latched ? 1U : 0U;

    const auto& latest = telemetry.latest;
    demo_debug_instance.dji_feedback_count = latest.feedback_count;
    demo_debug_instance.dji_last_feedback_tick = latest.last_tick;
    demo_debug_instance.dji_online = latest.online ? 1U : 0U;
    demo_debug_instance.dji_encoder = latest.value.encoder;
    demo_debug_instance.dji_speed_rpm = latest.value.speed_rpm;
    demo_debug_instance.dji_current = latest.value.current;
    demo_debug_instance.dji_temperature =
        latest.value.temperature;
    demo_debug_instance.dji_motor_id =
        motors::dji::protocol::feedback_id_valid(latest.feedback_id)
            ? static_cast<std::uint8_t>(
                  latest.feedback_id -
                  motors::dji::protocol::feedback_id_first + 1U)
            : 0U;
}

#if PNX_F407_USB_ENABLED
void sync_usb() noexcept
{
    const bsp::usb::capabilities capabilities =
        bsp::usb::get_capabilities();
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    demo_debug_instance.usb_device_mode =
        capabilities.device_mode ? 1U : 0U;
    demo_debug_instance.usb_high_speed =
        capabilities.high_speed ? 1U : 0U;
    demo_debug_instance.usb_vbus_sense =
        capabilities.supports_vbus_sense ? 1U : 0U;
    demo_debug_instance.usb_max_packet_size =
        capabilities.max_packet_size;
    demo_debug_instance.usb_max_transfer_size =
        capabilities.max_transfer_size;
    demo_debug_instance.usb_tx_queue_depth =
        capabilities.tx_queue_depth;
    demo_debug_instance.usb_initialized =
        state.initialized ? 1U : 0U;
    demo_debug_instance.usb_connected =
        state.connected ? 1U : 0U;
    demo_debug_instance.usb_link_state =
        static_cast<std::uint32_t>(state.link);
    demo_debug_instance.usb_read_count = state.read_count;
    demo_debug_instance.usb_write_count = state.write_count;
    demo_debug_instance.usb_error_count = state.error_count;
    demo_debug_instance.usb_rx_bytes = state.rx_bytes;
    demo_debug_instance.usb_tx_bytes = state.tx_bytes;
    demo_debug_instance.usb_tx_drop_count = state.tx_drop_count;
    demo_debug_instance.usb_connect_count = state.connect_count;
    demo_debug_instance.usb_disconnect_count =
        state.disconnect_count;
}
#endif

} // namespace cboard::demo
