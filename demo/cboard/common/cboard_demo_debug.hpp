#pragma once

#include "bsp_can.hpp"
#include "dji_motor_service.hpp"
#include "tx_api.h"

#include <cstdint>

namespace cboard::demo
{

enum class kind : std::uint32_t
{
    board_smoke = 1,
    can_receive = 2,
    motor_safe = 3,
    usb_cdc = 4,
    pwm_servo_lab = 5,
    imu_bmi088_lab = 6,
    ist8310_mag_lab = 7,
};

struct debug_state
{
    std::uint32_t abi_version = 4;
    std::uint32_t demo_kind = 0;
    std::uint32_t start_status = 0;
    std::uint32_t threadx_started = 0;
    std::uint32_t heartbeat_count = 0;
    std::uint32_t threadx_tick = 0;
    std::uint32_t dwt_us_low = 0;
    std::uint32_t dwt_us_high = 0;
    std::uint32_t reset_count = 0;
    std::uint32_t fault_count = 0;
    std::uint32_t reset_flags_raw = 0;
    std::uint32_t reset_reason_mask = 0;
    std::uint32_t crash_valid = 0;
    std::uint32_t crash_from_previous_boot = 0;
    std::uint32_t crash_sequence = 0;
    std::uint32_t crash_kind = 0;
    std::uint32_t crash_pc = 0;
    std::uint32_t crash_lr = 0;
    std::uint32_t crash_cfsr = 0;
    std::uint32_t crash_hfsr = 0;
    std::uint32_t crash_mmfar = 0;
    std::uint32_t crash_bfar = 0;
    std::uint32_t crash_context = 0;
    std::uint32_t thread_info_status = 0;
    std::uint32_t thread_state = 0;
    std::uint32_t thread_run_count = 0;
    std::uint32_t thread_stack_size = 0;
    std::uint32_t thread_stack_used = 0;
    std::uint32_t thread_stack_free = 0;
    std::uint32_t byte_pool_status = 0;
    std::uint32_t byte_pool_available = 0;
    std::uint32_t byte_pool_fragments = 0;
    std::uint32_t indicator_status = 0;
    std::uint32_t uart_status = 0;
    std::uint32_t uart_tx_count = 0;
    std::uint32_t uart_error_count = 0;
    std::uint32_t uart_busy_count = 0;
    std::uint32_t can_rx_count = 0;
    std::uint32_t can_last_id = 0;
    std::uint32_t can_last_tick = 0;
    std::uint32_t can_error_count = 0;
    std::uint32_t can_bus_off_count = 0;
    std::uint32_t can_drop_count = 0;
    std::uint32_t can_bus_state = 0;
    std::uint32_t dji_feedback_count = 0;
    std::uint32_t dji_last_feedback_tick = 0;
    std::uint32_t dji_online = 0;
    std::uint16_t dji_encoder = 0;
    std::int16_t dji_speed_rpm = 0;
    std::int16_t dji_current = 0;
    std::uint8_t dji_temperature = 0;
    std::uint8_t dji_motor_id = 0;
    std::uint32_t motor_service_faults = 0;
    std::uint32_t motor_queue_overflow = 0;
    std::uint32_t motor_zero_tx_count = 0;
    std::uint32_t motor_nonzero_tx_count = 0;
    std::int16_t motor_requested_current = 0;
    std::int16_t motor_applied_current = 0;
    std::uint32_t motor_arm_state = 0;
    std::uint32_t motor_pulse_latched = 0;
    std::uint32_t usb_status = 0;
    std::uint32_t usb_identity_confirmed = 0;
    std::uint32_t usb_device_mode = 0;
    std::uint32_t usb_high_speed = 0;
    std::uint32_t usb_vbus_sense = 0;
    std::uint32_t usb_max_packet_size = 0;
    std::uint32_t usb_max_transfer_size = 0;
    std::uint32_t usb_tx_queue_depth = 0;
    std::uint32_t usb_initialized = 0;
    std::uint32_t usb_connected = 0;
    std::uint32_t usb_link_state = 0;
    std::uint32_t usb_read_count = 0;
    std::uint32_t usb_write_count = 0;
    std::uint32_t usb_error_count = 0;
    std::uint32_t usb_rx_bytes = 0;
    std::uint32_t usb_tx_bytes = 0;
    std::uint32_t usb_tx_drop_count = 0;
    std::uint32_t usb_connect_count = 0;
    std::uint32_t usb_disconnect_count = 0;
};

void set_dwt_time(std::uint64_t microseconds) noexcept;
void sync_system_diagnostics() noexcept;
void sync_threadx(TX_THREAD* thread) noexcept;
void sync_can(bsp::can::bus bus) noexcept;
void sync_motor(
    const modules::motor::service_telemetry& telemetry) noexcept;
#if PNX_F407_USB_ENABLED
void sync_usb() noexcept;
#endif

} // namespace cboard::demo

extern "C" {
extern volatile cboard::demo::debug_state demo_debug_instance;
extern volatile std::uint32_t pnx_motor_demo_arm_token;
}
