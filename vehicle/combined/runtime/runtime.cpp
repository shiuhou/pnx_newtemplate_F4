#include "vehicle/combined/runtime/runtime.hpp"

#include "vehicle/combined/control/output_arbiter.hpp"
#include "vehicle/combined/control/ps2_input_adapter.hpp"
#include "vehicle/combined/control/vision_command.hpp"

#include "vehicle/arm/control/gravity_feedforward.hpp"
#include "vehicle/arm/control/j1_manual_command.hpp"
#include "vehicle/arm/control/j1_stall_guard.hpp"
#include "vehicle/arm/control/j1_zero_reference.hpp"
#include "vehicle/arm/control/servo_map.hpp"
#include "vehicle/arm/runtime/servo_pwm_output.hpp"

#include <config.hpp>
#include <bsp_usart.hpp>
#include <djimotorhandler.hpp>
#include <msg.hpp>
#include <remoter.hpp>
#include <robot_config.hpp>
#include <tx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace vehicle::combined::runtime
{
namespace
{

constexpr std::uint32_t control_period_ticks = 5U;
constexpr float control_period_s = 0.005F;
constexpr std::uint32_t servo_period_us = 20000U;
constexpr UINT control_priority = 6U;
constexpr std::size_t control_stack_bytes = 4096U;
constexpr std::uint32_t remote_freshness_ticks = 120U;
constexpr UINT remote_ingest_priority = 4U;
constexpr std::size_t remote_ingest_stack_bytes = 768U;
constexpr std::size_t vision_dma_buffer_size = 64U;

static_assert(TX_TIMER_TICKS_PER_SECOND == 1000U,
              "Combined runtime requires a one-millisecond ThreadX tick");
static_assert(::config::feature::enable_dr16 !=
                  ::config::feature::enable_ps2,
              "Combined image requires exactly one receiver source");

chassis::controller_configuration chassis_controller_config_of(
    const chassis::configuration& config) noexcept
{
    return {
        config.geometry,
        config.manual,
        config.command_slew,
        config.motor_direction,
        config.pi,
    };
}

motors::m2006 front_left{robot::motors::front_left};
motors::m2006 front_right{robot::motors::front_right};
motors::m2006 rear_left{robot::motors::rear_left};
motors::m2006 rear_right{robot::motors::rear_right};
motors::m2006 j1_motor{robot::motors::j1};
std::array<motors::m2006*, 4U> chassis_motor_order{
    &front_left,
    &front_right,
    &rear_left,
    &rear_right,
};
motors::djimotorhandler& motor_handler =
    motors::djimotorhandler::instance();

TX_THREAD control_thread{};
alignas(8) std::uint8_t control_stack[control_stack_bytes]{};
CHAR control_thread_name[] = "combined control";
TX_THREAD remote_ingest_thread{};
alignas(8) std::uint8_t remote_ingest_stack[remote_ingest_stack_bytes]{};
CHAR remote_ingest_thread_name[] = "combined remote";

struct remote_ingest_snapshot {
    remoter::state state{};
    std::uint32_t sample_tick{};
    bool seen{};
};

chassis::configuration chassis_configuration{};
arm::configuration arm_configuration{};
chassis::controller chassis_controller{
    chassis_controller_config_of(chassis_configuration)};
chassis::runtime_policy chassis_policy{false, false, {}};
arm::runtime_policy arm_policy{false, false, {}};
arm::arm_safety_gate arm_safety{};
arm::j1_hold_gate j1_hold{};
arm::j1_manual_command j1_manual{
    arm_configuration.j1_position,
    arm_configuration.j1_manual_position_rate_rad_per_s,
    arm_configuration.j1_position_min_rad,
    arm_configuration.j1_position_max_rad,
    arm_configuration.j1_manual_deadband};
arm::j1_zero_reference j1_zero{};
arm::j1_stall_guard j1_stall{arm_configuration.j1_stall};
chassis::velocity_pi j1_velocity_controller{
    arm_configuration.j1_velocity};
arm::servo_map servo_control{arm_configuration.servos};
arm::servo_pwm_output j2_pwm{{bsp::pwm::none, servo_period_us}};
arm::servo_pwm_output j3_pwm{{bsp::pwm::none, servo_period_us}};
arm::servo_pwm_output j4_pwm{{bsp::pwm::none, servo_period_us}};
mode_router router{};
ps2_input_adapter input_adapter{};
vision_command_receiver vision_receiver{};
bsp::usart::dma_rx_storage<vision_dma_buffer_size> vision_rx_storage{};

msg::subscriber remote_subscriber{};
remote_ingest_snapshot shared_remote{};
telemetry runtime_telemetry{};

std::array<bool, 5U> registration_results{};
bool runtime_start_attempted{};
bool any_motor_registered{};
bool startup_zero_sent{};
bool watchdog_sampled{};
bool all_motors_online{};
chassis::watchdog_phase health_phase{};
std::uint32_t control_loop_count{};
std::uint32_t control_overrun_count{};
std::uint32_t fault_mask{};
arm::j1_manual_command_output previous_manual_command{};
std::int16_t previous_logical_current_raw{};

void vision_rx_callback(bsp::usart::port,
                        const bsp::usart::rx_frame& frame,
                        void* user_data) noexcept
{
    auto* const receiver =
        static_cast<vision_command_receiver*>(user_data);
    if (receiver != nullptr && frame.data != nullptr)
    {
        (void)receiver->push_from_isr(frame.data, frame.len);
    }
}

bool start_vision_uart() noexcept
{
    if constexpr (!::config::feature::enable_ps2)
    {
        return true;
    }
    if (::app::uart::vision == bsp::usart::none)
    {
        return false;
    }

    bsp::usart::line_config line{};
    line.baud_rate = 115200U;
    line.data_bits = bsp::usart::word_length::bits_8;
    line.stop = bsp::usart::stop_bits::one;
    line.parity_mode = bsp::usart::parity::none;
    line.enable_tx = true;
    line.enable_rx = true;
    if (bsp::usart::configure(::app::uart::vision, line) !=
            types::status::ok ||
        bsp::usart::init(::app::uart::vision, bsp::usart::mode::dma) !=
            types::status::ok)
    {
        return false;
    }
    return bsp::usart::start_rx_to_idle(
               ::app::uart::vision, vision_rx_storage.data(),
               vision_dma_buffer_size, vision_rx_callback,
               &vision_receiver, nullptr, nullptr,
               bsp::usart::rx_delivery::frame_snapshot) ==
           types::status::ok;
}

void latch_fault(runtime_fault fault) noexcept
{
    fault_mask |= static_cast<std::uint32_t>(fault);
}

runtime_fault faults() noexcept
{
    return static_cast<runtime_fault>(fault_mask);
}

bool combined_fault_latched() noexcept
{
    return fault_mask != 0U;
}

bool terminal_fault_latched() noexcept
{
    return combined_fault_latched() || chassis_policy.fault_latched() ||
           arm_policy.fault_latched();
}

void sync_subsystem_fault() noexcept
{
    if (chassis_policy.fault_latched() || arm_policy.fault_latched())
    {
        latch_fault(runtime_fault::subsystem_fault);
    }
}

void relax_chassis() noexcept
{
    for (motors::m2006* const motor : chassis_motor_order)
    {
        motor->relax();
    }
}

void set_chassis_current(
    const std::array<std::int16_t, 4U>& current_raw) noexcept
{
    for (std::size_t index = 0U; index < chassis_motor_order.size(); ++index)
    {
        chassis_motor_order[index]->set_current(current_raw[index]);
    }
}

void relax_all_motors() noexcept
{
    relax_chassis();
    j1_motor.relax();
}

bool stop_servo_outputs() noexcept
{
    const bool j2_stopped = j2_pwm.update(false, 0U);
    const bool j3_stopped = j3_pwm.update(false, 0U);
    const bool j4_stopped = j4_pwm.update(false, 0U);
    return j2_stopped && j3_stopped && j4_stopped;
}

void publish_telemetry(const telemetry& next) noexcept
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    runtime_telemetry = next;
    TX_RESTORE
}

remote_ingest_snapshot copy_remote_snapshot() noexcept
{
    remote_ingest_snapshot copy{};
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    copy = shared_remote;
    TX_RESTORE
    return copy;
}

void publish_remote_snapshot(const remote_ingest_snapshot& next) noexcept
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    shared_remote = next;
    TX_RESTORE
}

void reset_arm_control() noexcept
{
    arm_safety.reset();
    j1_manual.reset();
    j1_stall.reset();
    j1_velocity_controller.reset();
    j1_hold.reset();
    servo_control.reset();
    previous_manual_command = {};
    previous_logical_current_raw = 0;
}

void latch_overrun() noexcept
{
    latch_fault(runtime_fault::overrun);
    chassis_policy.latch(chassis::runtime_fault::overrun);
    arm_policy.latch(arm::runtime_fault::overrun);
    chassis_controller.reset();
    reset_arm_control();
    router.reset();
    if (!stop_servo_outputs())
    {
        latch_fault(runtime_fault::pwm_failed);
        arm_policy.latch(arm::runtime_fault::pwm_failed);
    }
    relax_all_motors();
    ++control_overrun_count;
}

void send_zero_frame_now(const bsp::can::telemetry& can) noexcept
{
    relax_all_motors();
    if (any_motor_registered && can.bus_state == bsp::can::state::active)
    {
        motor_handler.send_control();
    }
}

void send_startup_zero_if_possible(
    const bsp::can::telemetry& can) noexcept
{
    if (!startup_zero_sent)
    {
        send_zero_frame_now(can);
        startup_zero_sent = true;
    }
}

void sync_fault_telemetry(const bsp::can::telemetry& can) noexcept
{
    sync_subsystem_fault();
    telemetry next{};
    next.faults = faults();
    next.watchdog_sampled = watchdog_sampled;
    next.all_motors_online = all_motors_online;
    next.chassis.state = chassis_policy.reported_state(
        chassis::safety_state::disabled);
    next.chassis.faults = chassis_policy.faults();
    next.chassis.watchdog_sampled = watchdog_sampled;
    next.chassis.can = can;
    next.arm.state = arm_policy.reported_state(
        arm::arm_safety_state::disabled);
    next.arm.faults = arm_policy.faults();
    next.arm.watchdog_sampled = watchdog_sampled;
    next.arm.j1_zero_captured = j1_zero.captured();
    next.arm.can = can;
    next.can = can;
    publish_telemetry(next);
}

void terminal_startup_failure(runtime_fault fault) noexcept
{
    latch_fault(fault);
    const auto can = bsp::can::snapshot(bsp::can::bus::can1);
    sync_fault_telemetry(can);
    (void)stop_servo_outputs();
    send_startup_zero_if_possible(can);
}

void sleep_until(std::uint32_t deadline) noexcept
{
    const std::uint32_t now = static_cast<std::uint32_t>(tx_time_get());
    const std::int32_t remaining =
        static_cast<std::int32_t>(deadline - now);
    if (remaining > 0)
    {
        tx_thread_sleep(static_cast<ULONG>(remaining));
    }
}

void remote_ingest_entry(ULONG)
{
    for (;;)
    {
        remoter::state latest{};
        if (msg::read(remote_subscriber, latest) == types::status::ok)
        {
            remote_ingest_snapshot next{};
            next.state = latest;
            next.sample_tick = static_cast<std::uint32_t>(tx_time_get());
            next.seen = true;
            publish_remote_snapshot(next);
        }
        else
        {
            tx_thread_sleep(1U);
        }
    }
}

void control_entry(ULONG)
{
    ::remoter::config remote_config{};
    if constexpr (::config::feature::enable_ps2)
    {
        remote_config.ps2.thread_priority = params::remoter::thread_priority;
        remote_config.ps2.receiver_offline_timeout_ticks =
            params::remoter::ps2_offline_timeout_ticks;
        remote_config.ps2.frame_timeout_ticks =
            params::remoter::ps2_frame_timeout_ticks;
        remote_config.ps2.deadzone = params::remoter::ps2_deadzone;
    }
    else
    {
        remote_config.dr16.thread_priority = params::remoter::thread_priority;
        remote_config.dr16.rx_timeout_ticks = params::remoter::rx_timeout_ticks;
    }
    remote_config.thread_priority = params::remoter::thread_priority + 1U;
    remote_config.offline_timeout_ticks = remote_freshness_ticks;

    if (!::remoter::service::instance().init(remote_config))
    {
        terminal_startup_failure(runtime_fault::remoter_init_failed);
        return;
    }

    if (!start_vision_uart())
    {
        terminal_startup_failure(runtime_fault::vision_uart_init_failed);
        return;
    }

    remote_subscriber = msg::subscribe<::remoter::state>();
    if (!remote_subscriber.valid())
    {
        terminal_startup_failure(runtime_fault::subscribe_failed);
        return;
    }

    if (tx_thread_create(
            &remote_ingest_thread, remote_ingest_thread_name,
            remote_ingest_entry, 0U, remote_ingest_stack,
            sizeof(remote_ingest_stack), remote_ingest_priority,
            remote_ingest_priority, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS)
    {
        terminal_startup_failure(runtime_fault::thread_create_failed);
        return;
    }

    std::uint32_t next_deadline =
        static_cast<std::uint32_t>(tx_time_get()) + control_period_ticks;

    for (;;)
    {
        const auto remote_snapshot = copy_remote_snapshot();
        const std::uint32_t now =
            static_cast<std::uint32_t>(tx_time_get());
        vision_receiver.process(now);
        const auto vision_snapshot = vision_receiver.snapshot();
        remoter::state control_remote = remote_snapshot.state;
        if (!chassis::remote_snapshot_fresh(
                remote_snapshot.seen, remote_snapshot.sample_tick,
                now, remote_freshness_ticks))
        {
            control_remote.offline = true;
            control_remote.active_source = remoter::source::none;
        }

        const auto adapted_remote = input_adapter.update(control_remote);
        const float arm_center_deadband = std::min(
            arm_configuration.j1_manual_deadband,
            arm_configuration.servos.deadband);
        const auto routed = router.update(
            adapted_remote, chassis_configuration.manual.deadband,
            arm_center_deadband);
        const auto can = bsp::can::snapshot(bsp::can::bus::can1);
        const std::uint32_t cycle = control_loop_count + 1U;
        chassis::wheel_vector measured_wheels{};
        float directed_position_rad = 0.0F;
        float directed_velocity_rad_s = 0.0F;
        const bool sample_watchdog = health_phase.advance();

        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        for (std::size_t index = 0U;
             index < chassis_motor_order.size(); ++index)
        {
            measured_wheels.rad_s[index] =
                chassis_motor_order[index]->get_feedback().velocity;
        }
        const auto j1_feedback = j1_motor.get_feedback();
        directed_position_rad =
            j1_feedback.position * arm_configuration.j1_motor_direction;
        directed_velocity_rad_s =
            j1_feedback.velocity * arm_configuration.j1_motor_direction;
        if (sample_watchdog)
        {
            all_motors_online = motor_handler.alive_check();
            watchdog_sampled = true;
        }
        TX_RESTORE

        chassis::runtime_policy_input chassis_input{};
        chassis_input.remote = routed.chassis_remote;
        chassis_input.can = can;
        chassis_input.watchdog_sampled = watchdog_sampled;
        chassis_input.handler_all_online = all_motors_online;
        auto chassis_policy_output = chassis_policy.update(chassis_input);

        arm::runtime_policy_input arm_input{};
        arm_input.remote = adapted_remote;
        arm_input.can = can;
        arm_input.watchdog_sampled = watchdog_sampled;
        arm_input.j1_online = all_motors_online;
        arm_input.manual_axes_centered = routed.arm_axes_centered;
        auto arm_policy_output = arm_policy.update(arm_input);
        sync_subsystem_fault();

        auto chassis_safety =
            chassis::controller_safety_for(chassis_policy_output);
        auto arm_controller_safety =
            arm::controller_safety_for(arm_policy_output);
        if (combined_fault_latched())
        {
            chassis_safety.config_valid = false;
            arm_controller_safety.config_valid = false;
        }
        const auto chassis_output = chassis_controller.update(
            chassis_policy_output.manual, measured_wheels,
            chassis_safety, control_period_s);
        const auto arm_controller_state =
            arm_safety.update(arm_controller_safety);

        if (chassis::trusted_release_observed(chassis_policy_output))
        {
            chassis_controller.reset();
        }
        if (arm::trusted_release_observed(arm_policy_output))
        {
            arm_safety.reset();
            j1_manual.reset();
            j1_stall.reset();
            j1_velocity_controller.reset();
            servo_control.reset();
            previous_manual_command = {};
            previous_logical_current_raw = 0;
        }

        const bool common_healthy =
            routed.remote_online && watchdog_sampled && all_motors_online &&
            chassis_policy_output.safety.can_healthy &&
            arm_policy_output.safety.can_healthy &&
            chassis_policy_output.safety.config_valid &&
            arm_policy_output.safety.config_valid;
        bool terminal_fault = terminal_fault_latched();
        const auto selected_outputs = select_outputs({
            routed.mode,
            routed.chassis_ready,
            common_healthy,
            terminal_fault,
            chassis::should_set_current(
                chassis_policy_output, chassis_output.state),
            arm::should_enable_outputs(
                arm_policy_output, arm_controller_state),
        });
        bool chassis_outputs_enabled = selected_outputs.chassis_enabled;
        bool arm_outputs_enabled = selected_outputs.arm_manual_enabled;
        bool j1_outputs_enabled = j1_hold.update({
            arm_outputs_enabled,
            arm_policy_output.manual.online,
            arm_policy_output.manual.right_switch_up,
            common_healthy,
            chassis_policy_output.safety.can_healthy &&
                arm_policy_output.safety.can_healthy,
            chassis_policy_output.safety.config_valid &&
                arm_policy_output.safety.config_valid,
            terminal_fault || !selected_outputs.arm_hold_allowed,
        });

        if (j1_outputs_enabled && !j1_zero.capture(directed_position_rad))
        {
            j1_hold.reset();
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
        }
        const float measured_position_rad =
            j1_zero.logical_position(directed_position_rad);
        if (!j1_outputs_enabled)
        {
            j1_manual.reset();
            j1_stall.reset();
            j1_velocity_controller.reset();
            previous_manual_command = {};
            previous_logical_current_raw = 0;
        }

        float stall_axis = arm_outputs_enabled
                               ? arm_policy_output.manual.j1_axis
                               : 0.0F;
        if (std::isfinite(stall_axis) &&
            std::fabs(stall_axis) <=
                arm_configuration.j1_manual_deadband)
        {
            stall_axis = 0.0F;
        }
        const auto stall_output = j1_stall.update({
            j1_outputs_enabled,
            stall_axis,
            previous_manual_command.target_position_rad,
            measured_position_rad,
            directed_velocity_rad_s,
            previous_logical_current_raw,
            control_period_s,
        });
        if (stall_output.hold_target)
        {
            j1_manual.reset();
            j1_velocity_controller.reset();
        }
        const auto manual_command = j1_manual.update({
            j1_outputs_enabled,
            stall_output.allowed_manual_axis,
            measured_position_rad,
            directed_velocity_rad_s,
            control_period_s,
        });

        std::int16_t feedback_current_raw = 0;
        float gravity_feedforward_raw = 0.0F;
        std::int16_t logical_current_raw = 0;
        std::int16_t j1_current_raw = 0;
        if (j1_outputs_enabled)
        {
            feedback_current_raw = j1_velocity_controller.update(
                manual_command.target_velocity_rad_s,
                directed_velocity_rad_s, control_period_s);
            gravity_feedforward_raw = arm::gravity_current_raw(
                measured_position_rad,
                arm_configuration.j1_gravity_amplitude_raw,
                arm_configuration.j1_gravity_phase_rad,
                arm_configuration.j1_gravity_bias_raw);
            logical_current_raw = arm::combine_current_raw(
                feedback_current_raw, gravity_feedforward_raw,
                arm_configuration.j1_velocity.current_limit_raw);
            j1_current_raw = static_cast<std::int16_t>(
                static_cast<float>(logical_current_raw) *
                arm_configuration.j1_motor_direction);
        }
        previous_manual_command = manual_command;
        previous_logical_current_raw =
            j1_outputs_enabled ? logical_current_raw : 0;

        bool iteration_overrun = chassis::deadline_reached(
            static_cast<std::uint32_t>(tx_time_get()), next_deadline);
        if (iteration_overrun)
        {
            latch_overrun();
            terminal_fault = true;
            chassis_outputs_enabled = false;
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
            j1_current_raw = 0;
        }

        const bool j2_hold_active =
            !arm_outputs_enabled && selected_outputs.arm_hold_allowed &&
            arm::should_hold_j2_output(j2_pwm.enabled(), arm_policy_output);
        const bool j3_hold_active =
            !arm_outputs_enabled && selected_outputs.arm_hold_allowed &&
            arm::should_hold_j2_output(j3_pwm.enabled(), arm_policy_output);
        const bool j4_hold_active =
            !arm_outputs_enabled && selected_outputs.arm_hold_allowed &&
            arm::should_hold_j2_output(j4_pwm.enabled(), arm_policy_output);
        const bool j2_control_enabled =
            arm_outputs_enabled || j2_hold_active;
        const bool j3_control_enabled =
            arm_outputs_enabled || j3_hold_active;
        const bool j4_control_enabled =
            arm_outputs_enabled || j4_hold_active;
        const bool servo_control_enabled =
            j2_control_enabled || j3_control_enabled || j4_control_enabled;
        const auto servo_command = servo_control.update({
            servo_control_enabled,
            arm_outputs_enabled ? arm_policy_output.manual.j2_axis : 0.0F,
            arm_outputs_enabled ? arm_policy_output.manual.j3_axis : 0.0F,
            arm_outputs_enabled
                ? arm_policy_output.manual.gripper_axis
                : 0.0F,
        }, control_period_s);
        const std::uint32_t j2_pulse_us =
            servo_command.pulse_us[arm::servo_index(arm::servo_axis::j2_pitch)];
        const std::uint32_t j3_pulse_us =
            servo_command.pulse_us[arm::servo_index(arm::servo_axis::j3_yaw)];
        const std::uint32_t j4_pulse_us =
            servo_command.pulse_us[arm::servo_index(arm::servo_axis::gripper)];
        const bool j2_output_enabled =
            j2_control_enabled && j2_pulse_us != 0U;
        const bool j3_output_enabled =
            j3_control_enabled && j3_pulse_us != 0U;
        const bool j4_output_enabled =
            j4_control_enabled && j4_pulse_us != 0U;
        const bool j2_update_ok =
            j2_pwm.update(j2_output_enabled, j2_pulse_us);
        const bool j3_update_ok =
            j3_pwm.update(j3_output_enabled, j3_pulse_us);
        const bool j4_update_ok =
            j4_pwm.update(j4_output_enabled, j4_pulse_us);
        if (!j2_update_ok || !j3_update_ok || !j4_update_ok)
        {
            latch_fault(runtime_fault::pwm_failed);
            arm_policy.latch(arm::runtime_fault::pwm_failed);
            sync_subsystem_fault();
            chassis_controller.reset();
            reset_arm_control();
            (void)stop_servo_outputs();
            terminal_fault = true;
            chassis_outputs_enabled = false;
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
            j1_current_raw = 0;
        }

        if (!iteration_overrun && chassis::deadline_reached(
                static_cast<std::uint32_t>(tx_time_get()), next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
            terminal_fault = true;
            chassis_outputs_enabled = false;
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
            j1_current_raw = 0;
        }

        if (chassis_outputs_enabled && !terminal_fault)
        {
            set_chassis_current(chassis_output.motor_current_raw);
        }
        else
        {
            relax_chassis();
        }
        if (j1_outputs_enabled && !terminal_fault)
        {
            j1_motor.set_current(j1_current_raw);
        }
        else
        {
            j1_motor.relax();
        }

        motor_handler.send_control();

        const std::uint32_t after_send =
            static_cast<std::uint32_t>(tx_time_get());
        if (!iteration_overrun &&
            chassis::deadline_reached(after_send, next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
            terminal_fault = true;
            chassis_outputs_enabled = false;
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
            j1_current_raw = 0;
            send_zero_frame_now(can);
        }

        telemetry next_telemetry{};
        next_telemetry.mode = routed.mode;
        next_telemetry.active_source = control_remote.active_source;
        next_telemetry.ps2_link = control_remote.ps2_link;
        next_telemetry.ps2_buttons = control_remote.ps2_buttons;
        next_telemetry.ps2_pressed = control_remote.ps2_pressed;
        next_telemetry.ps2_unlocked =
            adapted_remote.active_source == remoter::source::ps2 &&
            !adapted_remote.offline &&
            adapted_remote.right_sw == remoter::sw_state::up;
        next_telemetry.r1_chassis_held =
            next_telemetry.ps2_unlocked &&
            remoter::is_held(control_remote.ps2_buttons,
                             remoter::ps2_button::r1);
        next_telemetry.r2_arm_held =
            next_telemetry.ps2_unlocked &&
            remoter::is_held(control_remote.ps2_buttons,
                             remoter::ps2_button::r2);
        next_telemetry.faults = faults();
        next_telemetry.watchdog_sampled = watchdog_sampled;
        next_telemetry.all_motors_online = all_motors_online;
        next_telemetry.loop_count = cycle;
        next_telemetry.overrun_count = control_overrun_count;
        next_telemetry.remote_update_count = control_remote.update_count;
        next_telemetry.vision = vision_snapshot;
        next_telemetry.can = can;

        next_telemetry.chassis.state =
            chassis_policy.reported_state(chassis_output.state);
        next_telemetry.chassis.faults = chassis_policy.faults();
        next_telemetry.chassis.watchdog_sampled = watchdog_sampled;
        next_telemetry.chassis.loop_count = cycle;
        next_telemetry.chassis.overrun_count = control_overrun_count;
        next_telemetry.chassis.remote_update_count =
            control_remote.update_count;
        next_telemetry.chassis.target_rad_s =
            chassis_output.wheel_target_rad_s.rad_s;
        next_telemetry.chassis.measured_rad_s = measured_wheels.rad_s;
        next_telemetry.chassis.current_raw =
            chassis_outputs_enabled && !terminal_fault
                ? chassis_output.motor_current_raw
                : std::array<std::int16_t, 4U>{};
        next_telemetry.chassis.can = can;

        next_telemetry.arm.state =
            arm_policy.reported_state(arm_controller_state);
        next_telemetry.arm.faults = arm_policy.faults();
        next_telemetry.arm.watchdog_sampled = watchdog_sampled;
        next_telemetry.arm.loop_count = cycle;
        next_telemetry.arm.overrun_count = control_overrun_count;
        next_telemetry.arm.remote_update_count =
            control_remote.update_count;
        next_telemetry.arm.mode = arm_policy_output.manual.mode;
        next_telemetry.arm.j1_zero_captured = j1_zero.captured();
        next_telemetry.arm.j1_axis = arm_policy_output.manual.j1_axis;
        next_telemetry.arm.j1_target_position_rad =
            manual_command.target_position_rad;
        next_telemetry.arm.j1_measured_position_rad = measured_position_rad;
        next_telemetry.arm.j1_target_velocity_rad_s =
            manual_command.target_velocity_rad_s;
        next_telemetry.arm.j1_measured_velocity_rad_s =
            directed_velocity_rad_s;
        next_telemetry.arm.j1_feedback_current_raw =
            j1_outputs_enabled && !terminal_fault
                ? feedback_current_raw
                : 0;
        next_telemetry.arm.j1_gravity_current_raw =
            j1_outputs_enabled && !terminal_fault
                ? gravity_feedforward_raw
                : 0.0F;
        next_telemetry.arm.j1_current_raw =
            j1_outputs_enabled && !terminal_fault ? j1_current_raw : 0;
        next_telemetry.arm.j1_stall_elapsed_s =
            stall_output.stall_elapsed_s;
        next_telemetry.arm.j1_stall_blocked_direction =
            stall_output.blocked_direction;
        next_telemetry.arm.j1_stall_active =
            stall_output.blocked_direction != arm::j1_stall_direction::none;
        next_telemetry.arm.j1_hold_active =
            j1_outputs_enabled && !arm_outputs_enabled && !terminal_fault;
        next_telemetry.arm.j2_axis = arm_policy_output.manual.j2_axis;
        next_telemetry.arm.j2_pulse_us = j2_pwm.pulse_us();
        next_telemetry.arm.j2_pwm_enabled = j2_pwm.enabled();
        next_telemetry.arm.j3_axis = arm_policy_output.manual.j3_axis;
        next_telemetry.arm.j3_pulse_us = j3_pwm.pulse_us();
        next_telemetry.arm.j3_pwm_enabled = j3_pwm.enabled();
        next_telemetry.arm.gripper_axis =
            arm_policy_output.manual.gripper_axis;
        next_telemetry.arm.gripper_pulse_us = j4_pwm.pulse_us();
        next_telemetry.arm.gripper_pwm_enabled = j4_pwm.enabled();
        next_telemetry.arm.outputs_enabled =
            j1_outputs_enabled && !terminal_fault;
        next_telemetry.arm.can = can;
        publish_telemetry(next_telemetry);
        control_loop_count = cycle;

        if (iteration_overrun)
        {
            next_deadline =
                static_cast<std::uint32_t>(tx_time_get()) +
                control_period_ticks;
        }
        sleep_until(next_deadline);
        next_deadline += control_period_ticks;
    }
}

} // namespace

void start(const chassis::configuration& chassis_config,
           const arm::configuration& arm_config) noexcept
{
    if (runtime_start_attempted)
    {
        return;
    }
    runtime_start_attempted = true;

    chassis_configuration = chassis_config;
    arm_configuration = arm_config;
    chassis_controller = chassis::controller{
        chassis_controller_config_of(chassis_config)};
    j1_manual = arm::j1_manual_command{
        arm_config.j1_position,
        arm_config.j1_manual_position_rate_rad_per_s,
        arm_config.j1_position_min_rad,
        arm_config.j1_position_max_rad,
        arm_config.j1_manual_deadband};
    j1_zero = {};
    j1_stall = arm::j1_stall_guard{arm_config.j1_stall};
    j1_velocity_controller =
        chassis::velocity_pi{arm_config.j1_velocity};
    servo_control = arm::servo_map{arm_config.servos};
    const auto& j2_config =
        arm_config.servos.axes[arm::servo_index(arm::servo_axis::j2_pitch)];
    j2_pwm = arm::servo_pwm_output{{j2_config.channel, servo_period_us}};
    const auto& j3_config =
        arm_config.servos.axes[arm::servo_index(arm::servo_axis::j3_yaw)];
    j3_pwm = arm::servo_pwm_output{{j3_config.channel, servo_period_us}};
    const auto& j4_config =
        arm_config.servos.axes[arm::servo_index(arm::servo_axis::gripper)];
    j4_pwm = arm::servo_pwm_output{{j4_config.channel, servo_period_us}};

    fault_mask = 0U;
    chassis_policy = chassis::runtime_policy{false, false, {}};
    arm_policy = arm::runtime_policy{false, false, {}};
    arm_safety = {};
    j1_hold = {};
    router.reset();
    input_adapter.reset();
    vision_receiver.reset();
    remote_subscriber = {};
    publish_remote_snapshot({});
    publish_telemetry({});
    registration_results = {};
    any_motor_registered = false;
    startup_zero_sent = false;
    watchdog_sampled = false;
    all_motors_online = false;
    health_phase = {};
    control_loop_count = 0U;
    control_overrun_count = 0U;
    previous_manual_command = {};
    previous_logical_current_raw = 0;

    registration_results[0] = motor_handler.register_motor(front_left);
    registration_results[1] = motor_handler.register_motor(front_right);
    registration_results[2] = motor_handler.register_motor(rear_left);
    registration_results[3] = motor_handler.register_motor(rear_right);
    registration_results[4] = motor_handler.register_motor(j1_motor);
    any_motor_registered = std::any_of(
        registration_results.begin(), registration_results.end(),
        [](bool registered) { return registered; });
    const bool all_registered = std::all_of(
        registration_results.begin(), registration_results.end(),
        [](bool registered) { return registered; });
    const auto can_baseline =
        bsp::can::snapshot(bsp::can::bus::can1);
    const bool configs_valid =
        chassis::valid(chassis_configuration) &&
        arm::valid(arm_configuration) &&
        chassis_configuration.control_period_s == control_period_s &&
        arm_configuration.control_period_s == control_period_s;

    chassis_policy = chassis::runtime_policy{
        configs_valid, all_registered, can_baseline};
    arm_policy = arm::runtime_policy{
        configs_valid, all_registered, can_baseline};
    if (!configs_valid)
    {
        latch_fault(runtime_fault::invalid_config);
    }
    if (!all_registered)
    {
        latch_fault(runtime_fault::registration_failed);
    }
    sync_subsystem_fault();
    sync_fault_telemetry(can_baseline);
    send_startup_zero_if_possible(can_baseline);

    if (!all_registered)
    {
        return;
    }

    if (tx_thread_create(
            &control_thread, control_thread_name, control_entry, 0U,
            control_stack, sizeof(control_stack), control_priority,
            control_priority, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS)
    {
        terminal_startup_failure(runtime_fault::thread_create_failed);
    }
}

telemetry debug_state() noexcept
{
    telemetry copy{};
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    copy = runtime_telemetry;
    TX_RESTORE
    return copy;
}

} // namespace vehicle::combined::runtime
