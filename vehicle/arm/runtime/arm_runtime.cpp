#include "vehicle/arm/runtime/arm_runtime.hpp"

#include "vehicle/arm/control/gravity_feedforward.hpp"
#include "vehicle/arm/control/j1_manual_command.hpp"
#include "vehicle/arm/control/j1_stall_guard.hpp"
#include "vehicle/arm/control/j1_zero_reference.hpp"
#include "vehicle/arm/control/servo_map.hpp"
#include "vehicle/arm/runtime/servo_pwm_output.hpp"

#include <config.hpp>
#include <djimotorhandler.hpp>
#include <msg.hpp>
#include <remoter.hpp>
#include <robot_config.hpp>
#include <tx_api.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace vehicle::arm::runtime
{
namespace
{

// DR16 -> safety policy -> J1 closed loop and bounded J2/J3/J4 PWM commands.
constexpr std::uint32_t control_period_ticks = 5U;
constexpr float control_period_s = 0.005F;
constexpr std::uint32_t servo_period_us = 20000U;
constexpr UINT control_priority = 6U;
constexpr std::size_t control_stack_bytes = 2048U;
constexpr std::uint32_t remote_freshness_ticks = 120U;
constexpr UINT remote_ingest_priority = 4U;
constexpr std::size_t remote_ingest_stack_bytes = 768U;

static_assert(TX_TIMER_TICKS_PER_SECOND == 1000U,
              "Arm runtime requires a one-millisecond ThreadX tick");

motors::m2006 j1_motor{robot::motors::j1};
motors::djimotorhandler& motor_handler =
    motors::djimotorhandler::instance();

TX_THREAD control_thread{};
alignas(8) std::uint8_t control_stack[control_stack_bytes]{};
CHAR control_thread_name[] = "arm control";
TX_THREAD remote_ingest_thread{};
alignas(8) std::uint8_t remote_ingest_stack[remote_ingest_stack_bytes]{};
CHAR remote_ingest_thread_name[] = "arm remote";

struct remote_ingest_snapshot {
    remoter::state state{};
    std::uint32_t sample_tick{};
    bool seen{};
};

configuration runtime_configuration{};
j1_manual_command j1_manual{
    runtime_configuration.j1_position,
    runtime_configuration.j1_manual_position_rate_rad_per_s,
    runtime_configuration.j1_position_min_rad,
    runtime_configuration.j1_position_max_rad,
    runtime_configuration.j1_manual_deadband};
j1_zero_reference j1_zero{};
j1_stall_guard j1_stall{runtime_configuration.j1_stall};
::vehicle::chassis::velocity_pi j1_velocity_controller{
    runtime_configuration.j1_velocity};
servo_map servo_control{runtime_configuration.servos};
servo_pwm_output j2_pwm{{bsp::pwm::none, servo_period_us}};
servo_pwm_output j3_pwm{{bsp::pwm::none, servo_period_us}};
servo_pwm_output j4_pwm{{bsp::pwm::none, servo_period_us}};
runtime_policy policy{false, false, {}};
arm_safety_gate runtime_safety{};
j1_hold_gate j1_hold{};
msg::subscriber remote_subscriber{};
remote_ingest_snapshot shared_remote{};
telemetry runtime_telemetry{};

bool runtime_start_attempted{};
bool j1_registered{};
bool startup_zero_sent{};
bool watchdog_sampled{};
bool j1_online{};
watchdog_phase health_phase{};
std::uint32_t control_loop_count{};
std::uint32_t control_overrun_count{};
j1_manual_command_output previous_manual_command{};
std::int16_t previous_logical_current_raw{};

void relax_j1() noexcept
{
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

void publish_remote_snapshot(
    const remote_ingest_snapshot& next) noexcept
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    shared_remote = next;
    TX_RESTORE
}

void sync_fault_telemetry(const bsp::can::telemetry& can) noexcept
{
    telemetry next{};
    next.state = policy.reported_state(arm_safety_state::disabled);
    next.faults = policy.faults();
    next.watchdog_sampled = watchdog_sampled;
    next.j1_zero_captured = j1_zero.captured();
    next.outputs_enabled = false;
    next.can = can;
    publish_telemetry(next);
}

void latch_overrun() noexcept
{
    policy.latch(runtime_fault::overrun);
    runtime_safety.reset();
    j1_manual.reset();
    j1_stall.reset();
    j1_velocity_controller.reset();
    j1_hold.reset();
    servo_control.reset();
    if (!stop_servo_outputs())
    {
        policy.latch(runtime_fault::pwm_failed);
    }
    previous_manual_command = {};
    previous_logical_current_raw = 0;
    ++control_overrun_count;
}

void send_startup_zero_if_possible(
    const bsp::can::telemetry& can) noexcept
{
    relax_j1();
    if (!startup_zero_sent && j1_registered &&
        can.bus_state == bsp::can::state::active)
    {
        motor_handler.send_control();
        startup_zero_sent = true;
    }
}

void terminal_startup_failure(runtime_fault fault) noexcept
{
    policy.latch(fault);
    const auto can = bsp::can::snapshot(bsp::can::bus::can1);
    runtime_policy_input observation{};
    observation.can = can;
    (void)policy.update(observation);
    sync_fault_telemetry(can);
    send_startup_zero_if_possible(can);
}

void sleep_until(std::uint32_t deadline) noexcept
{
    const std::uint32_t now =
        static_cast<std::uint32_t>(tx_time_get());
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
            next.sample_tick =
                static_cast<std::uint32_t>(tx_time_get());
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
    ::remoter::config config{};
    config.dr16.thread_priority = params::remoter::thread_priority;
    config.dr16.rx_timeout_ticks = params::remoter::rx_timeout_ticks;
    config.thread_priority = params::remoter::thread_priority + 1U;
    config.offline_timeout_ticks = remote_freshness_ticks;

    if (!::remoter::service::instance().init(config))
    {
        terminal_startup_failure(runtime_fault::remoter_init_failed);
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
        remoter::state control_remote = remote_snapshot.state;
        if (!remote_snapshot_fresh(
                remote_snapshot.seen, remote_snapshot.sample_tick,
                now, remote_freshness_ticks))
        {
            control_remote.offline = true;
            control_remote.active_source = remoter::source::none;
        }
        const auto can = bsp::can::snapshot(bsp::can::bus::can1);
        const std::uint32_t cycle = control_loop_count + 1U;
        float directed_position_rad = 0.0F;
        float directed_velocity_rad_s = 0.0F;
        const bool sample_watchdog = health_phase.advance();

        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        const auto feedback = j1_motor.get_feedback();
        directed_position_rad =
            feedback.position * runtime_configuration.j1_motor_direction;
        directed_velocity_rad_s =
            feedback.velocity * runtime_configuration.j1_motor_direction;
        if (sample_watchdog)
        {
            j1_online = motor_handler.alive_check();
            watchdog_sampled = true;
        }
        TX_RESTORE

        runtime_policy_input policy_input{};
        policy_input.remote = control_remote;
        policy_input.can = can;
        policy_input.watchdog_sampled = watchdog_sampled;
        policy_input.j1_online = j1_online;
        policy_input.manual_axes_centered =
            std::isfinite(control_remote.right_y) &&
            std::fabs(control_remote.right_y) <=
                runtime_configuration.j1_manual_deadband &&
            std::isfinite(control_remote.right_x) &&
            std::fabs(control_remote.right_x) <=
                runtime_configuration.servos.deadband &&
            std::isfinite(control_remote.left_y) &&
            std::fabs(control_remote.left_y) <=
                runtime_configuration.servos.deadband &&
            std::isfinite(control_remote.left_x) &&
            std::fabs(control_remote.left_x) <=
                runtime_configuration.servos.deadband;
        auto policy_output = policy.update(policy_input);
        const auto controller_safety = controller_safety_for(policy_output);
        const auto controller_state = runtime_safety.update(controller_safety);
        if (trusted_release_observed(policy_output))
        {
            runtime_safety.reset();
            j1_manual.reset();
            j1_stall.reset();
            j1_velocity_controller.reset();
            servo_control.reset();
            previous_manual_command = {};
            previous_logical_current_raw = 0;
        }

        bool arm_outputs_enabled =
            should_enable_outputs(policy_output, controller_state);
        bool j1_outputs_enabled = j1_hold.update({
            arm_outputs_enabled,
            policy_output.manual.online,
            policy_output.manual.right_switch_up,
            policy_output.safety.j1_online,
            policy_output.safety.can_healthy,
            policy_output.safety.config_valid,
            policy_output.fault_latched,
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
                               ? policy_output.manual.j1_axis
                               : 0.0F;
        if (std::isfinite(stall_axis) &&
            std::fabs(stall_axis) <=
                runtime_configuration.j1_manual_deadband)
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
        std::int16_t current_raw = 0;
        if (j1_outputs_enabled)
        {
            feedback_current_raw = j1_velocity_controller.update(
                manual_command.target_velocity_rad_s,
                directed_velocity_rad_s, control_period_s);
            gravity_feedforward_raw = gravity_current_raw(
                measured_position_rad,
                runtime_configuration.j1_gravity_amplitude_raw,
                runtime_configuration.j1_gravity_phase_rad,
                runtime_configuration.j1_gravity_bias_raw);
            logical_current_raw = combine_current_raw(
                feedback_current_raw, gravity_feedforward_raw,
                runtime_configuration.j1_velocity.current_limit_raw);
            current_raw = static_cast<std::int16_t>(
                static_cast<float>(logical_current_raw) *
                runtime_configuration.j1_motor_direction);
        }
        previous_manual_command = manual_command;
        previous_logical_current_raw =
            j1_outputs_enabled ? logical_current_raw : 0;

        bool iteration_overrun = deadline_reached(
            static_cast<std::uint32_t>(tx_time_get()), next_deadline);
        if (iteration_overrun)
        {
            latch_overrun();
            policy_output.force_zero = true;
            policy_output.fault_latched = true;
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
            current_raw = 0;
        }

        const bool j2_hold_active =
            !arm_outputs_enabled &&
            should_hold_j2_output(j2_pwm.enabled(), policy_output);
        const bool j3_hold_active =
            !arm_outputs_enabled &&
            should_hold_j2_output(j3_pwm.enabled(), policy_output);
        const bool j4_hold_active =
            !arm_outputs_enabled &&
            should_hold_j2_output(j4_pwm.enabled(), policy_output);
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
            arm_outputs_enabled ? policy_output.manual.j2_axis : 0.0F,
            arm_outputs_enabled ? policy_output.manual.j3_axis : 0.0F,
            arm_outputs_enabled ? policy_output.manual.gripper_axis : 0.0F,
        }, control_period_s);
        const std::uint32_t j2_pulse_us =
            servo_command.pulse_us[servo_index(servo_axis::j2_pitch)];
        const std::uint32_t j3_pulse_us =
            servo_command.pulse_us[servo_index(servo_axis::j3_yaw)];
        const std::uint32_t j4_pulse_us =
            servo_command.pulse_us[servo_index(servo_axis::gripper)];
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
            policy.latch(runtime_fault::pwm_failed);
            runtime_safety.reset();
            j1_manual.reset();
            j1_stall.reset();
            j1_velocity_controller.reset();
            j1_hold.reset();
            servo_control.reset();
            (void)stop_servo_outputs();
            previous_manual_command = {};
            previous_logical_current_raw = 0;
            policy_output.force_zero = true;
            policy_output.fault_latched = true;
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
            current_raw = 0;
        }

        if (j1_outputs_enabled)
        {
            j1_motor.set_current(current_raw);
        }
        else
        {
            relax_j1();
        }

        motor_handler.send_control();

        const std::uint32_t after_send =
            static_cast<std::uint32_t>(tx_time_get());
        if (!iteration_overrun &&
            deadline_reached(after_send, next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
            arm_outputs_enabled = false;
            j1_outputs_enabled = false;
            current_raw = 0;
            relax_j1();
            motor_handler.send_control();
        }

        telemetry next_telemetry{};
        next_telemetry.watchdog_sampled = watchdog_sampled;
        next_telemetry.loop_count = cycle;
        next_telemetry.overrun_count = control_overrun_count;
        next_telemetry.remote_update_count = control_remote.update_count;
        next_telemetry.mode = policy_output.manual.mode;
        next_telemetry.j1_zero_captured = j1_zero.captured();
        next_telemetry.j1_axis = policy_output.manual.j1_axis;
        next_telemetry.j1_target_position_rad =
            manual_command.target_position_rad;
        next_telemetry.j1_measured_position_rad = measured_position_rad;
        next_telemetry.j1_target_velocity_rad_s =
            manual_command.target_velocity_rad_s;
        next_telemetry.j1_measured_velocity_rad_s = directed_velocity_rad_s;
        next_telemetry.j1_feedback_current_raw =
            j1_outputs_enabled ? feedback_current_raw : 0;
        next_telemetry.j1_gravity_current_raw =
            j1_outputs_enabled ? gravity_feedforward_raw : 0.0F;
        next_telemetry.j1_current_raw =
            j1_outputs_enabled ? current_raw : 0;
        next_telemetry.j1_stall_elapsed_s =
            stall_output.stall_elapsed_s;
        next_telemetry.j1_stall_blocked_direction =
            stall_output.blocked_direction;
        next_telemetry.j1_stall_active =
            stall_output.blocked_direction != j1_stall_direction::none;
        next_telemetry.j1_hold_active =
            j1_outputs_enabled && !arm_outputs_enabled;
        next_telemetry.j2_axis = policy_output.manual.j2_axis;
        next_telemetry.j2_pulse_us = j2_pwm.pulse_us();
        next_telemetry.j2_pwm_enabled = j2_pwm.enabled();
        next_telemetry.j3_axis = policy_output.manual.j3_axis;
        next_telemetry.j3_pulse_us = j3_pwm.pulse_us();
        next_telemetry.j3_pwm_enabled = j3_pwm.enabled();
        next_telemetry.gripper_axis = policy_output.manual.gripper_axis;
        next_telemetry.gripper_pulse_us = j4_pwm.pulse_us();
        next_telemetry.gripper_pwm_enabled = j4_pwm.enabled();
        next_telemetry.outputs_enabled = j1_outputs_enabled;
        next_telemetry.can = can;
        next_telemetry.state =
            policy.reported_state(controller_state);
        next_telemetry.faults = policy.faults();
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

void start(const configuration& config) noexcept
{
    if (runtime_start_attempted)
    {
        return;
    }
    runtime_start_attempted = true;

    runtime_configuration = config;
    j1_manual = j1_manual_command{
        config.j1_position,
        config.j1_manual_position_rate_rad_per_s,
        config.j1_position_min_rad,
        config.j1_position_max_rad,
        config.j1_manual_deadband};
    j1_zero = {};
    j1_stall = j1_stall_guard{config.j1_stall};
    j1_velocity_controller =
        ::vehicle::chassis::velocity_pi{config.j1_velocity};
    servo_control = servo_map{config.servos};
    const auto& j2_config =
        config.servos.axes[servo_index(servo_axis::j2_pitch)];
    j2_pwm = servo_pwm_output{{j2_config.channel, servo_period_us}};
    const auto& j3_config =
        config.servos.axes[servo_index(servo_axis::j3_yaw)];
    j3_pwm = servo_pwm_output{{j3_config.channel, servo_period_us}};
    const auto& j4_config =
        config.servos.axes[servo_index(servo_axis::gripper)];
    j4_pwm = servo_pwm_output{{j4_config.channel, servo_period_us}};
    policy = runtime_policy{false, false, {}};
    runtime_safety = {};
    j1_hold = {};
    remote_subscriber = {};
    publish_remote_snapshot({});
    publish_telemetry({});
    j1_registered = false;
    startup_zero_sent = false;
    watchdog_sampled = false;
    j1_online = false;
    health_phase = {};
    control_loop_count = 0U;
    control_overrun_count = 0U;
    previous_manual_command = {};
    previous_logical_current_raw = 0;

    j1_registered = motor_handler.register_motor(j1_motor);
    const auto can_baseline =
        bsp::can::snapshot(bsp::can::bus::can1);
    const bool config_valid =
        valid(runtime_configuration) &&
        runtime_configuration.control_period_s == control_period_s;
    policy = runtime_policy{config_valid, j1_registered, can_baseline};
    sync_fault_telemetry(can_baseline);

    if (!j1_registered)
    {
        send_startup_zero_if_possible(can_baseline);
        return;
    }

    send_startup_zero_if_possible(can_baseline);
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

} // namespace vehicle::arm::runtime
