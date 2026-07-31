#include "vehicle/chassis/runtime.hpp"

#include <config.hpp>
#include <djimotorhandler.hpp>
#include <msg.hpp>
#include <remoter.hpp>
#include <robot_config.hpp>
#include <tx_api.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace vehicle::chassis::runtime
{
namespace
{

constexpr std::uint32_t control_period_ticks = 5U;
constexpr float control_period_s = 0.005F;
constexpr UINT control_priority = 6U;
constexpr std::size_t control_stack_bytes = 1024U;
constexpr std::uint32_t remote_freshness_ticks = 120U;
constexpr UINT remote_ingest_priority = 4U;
constexpr std::size_t remote_ingest_stack_bytes = 768U;

static_assert(TX_TIMER_TICKS_PER_SECOND == 1000U,
              "MyCar runtime requires a one-millisecond ThreadX tick");

controller_configuration controller_config_of(
    const configuration& config) noexcept
{
    return {
        config.geometry,
        config.manual,
        config.motor_direction,
        config.pi,
    };
}

motors::m2006 front_left{robot::motors::front_left};
motors::m2006 front_right{robot::motors::front_right};
motors::m2006 rear_left{robot::motors::rear_left};
motors::m2006 rear_right{robot::motors::rear_right};
std::array<motors::m2006*, 4U> motor_order{
    &front_left,
    &front_right,
    &rear_left,
    &rear_right,
};
motors::djimotorhandler& motor_handler =
    motors::djimotorhandler::instance();

TX_THREAD control_thread{};
alignas(8) std::uint8_t control_stack[control_stack_bytes]{};
CHAR control_thread_name[] = "mycar control";
TX_THREAD remote_ingest_thread{};
alignas(8) std::uint8_t remote_ingest_stack[remote_ingest_stack_bytes]{};
CHAR remote_ingest_thread_name[] = "mycar remote";

struct remote_ingest_snapshot {
    remoter::state state{};
    std::uint32_t sample_tick{};
    bool seen{};
};

configuration runtime_configuration{};
controller runtime_controller{controller_config_of(runtime_configuration)};
runtime_policy policy{false, false, {}};
msg::subscriber remote_subscriber{};
remote_ingest_snapshot shared_remote{};
telemetry runtime_telemetry{};

std::array<bool, 4U> registration_results{};
bool any_motor_registered{};
bool runtime_start_attempted{};
bool startup_zero_sent{};
bool watchdog_sampled{};
bool handler_all_online{};
watchdog_phase motor_watchdog_phase{};
std::uint32_t control_loop_count{};
std::uint32_t control_overrun_count{};

void relax_all() noexcept
{
    for (motors::m2006* const motor : motor_order)
    {
        motor->relax();
    }
}

void set_all_current(
    const std::array<std::int16_t, 4U>& current_raw) noexcept
{
    for (std::size_t index = 0U; index < motor_order.size(); ++index)
    {
        motor_order[index]->set_current(current_raw[index]);
    }
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
    next.state = policy.reported_state(safety_state::disabled);
    next.faults = policy.faults();
    next.watchdog_sampled = watchdog_sampled;
    next.can = can;
    publish_telemetry(next);
}

void latch_overrun() noexcept
{
    policy.latch(runtime_fault::overrun);
    runtime_controller.reset();
    ++control_overrun_count;
}

void send_startup_zero_if_possible(
    const bsp::can::telemetry& can) noexcept
{
    relax_all();
    if (!startup_zero_sent && any_motor_registered &&
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
        wheel_vector measured{};
        const bool sample_watchdog = motor_watchdog_phase.advance();

        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        for (std::size_t index = 0U; index < motor_order.size(); ++index)
        {
            measured.rad_s[index] = motor_order[index]->get_feedback().velocity;
        }
        if (sample_watchdog)
        {
            handler_all_online = motor_handler.alive_check();
            watchdog_sampled = true;
        }
        TX_RESTORE

        runtime_policy_input policy_input{};
        policy_input.remote = control_remote;
        policy_input.can = can;
        policy_input.watchdog_sampled = watchdog_sampled;
        policy_input.handler_all_online = handler_all_online;
        auto policy_output = policy.update(policy_input);
        const auto controller_safety = controller_safety_for(policy_output);
        const auto controller_output = runtime_controller.update(
            policy_output.manual, measured, controller_safety,
            control_period_s);
        if (trusted_release_observed(policy_output))
        {
            runtime_controller.reset();
        }

        bool iteration_overrun = deadline_reached(
            static_cast<std::uint32_t>(tx_time_get()), next_deadline);
        if (iteration_overrun)
        {
            latch_overrun();
            policy_output.force_zero = true;
            policy_output.fault_latched = true;
        }

        const bool current_selected =
            should_set_current(policy_output, controller_output.state);
        if (current_selected)
        {
            set_all_current(controller_output.motor_current_raw);
        }
        else
        {
            relax_all();
        }

        // The pinned handler returns void. This records one call, not CAN
        // acceptance; a failed transmission can only affect a later snapshot.
        motor_handler.send_control();

        const std::uint32_t after_send =
            static_cast<std::uint32_t>(tx_time_get());
        if (!iteration_overrun &&
            deadline_reached(after_send, next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
        }

        telemetry next_telemetry{};
        next_telemetry.watchdog_sampled = watchdog_sampled;
        next_telemetry.loop_count = cycle;
        next_telemetry.overrun_count = control_overrun_count;
        next_telemetry.remote_update_count = control_remote.update_count;
        next_telemetry.target_rad_s =
            controller_output.wheel_target_rad_s.rad_s;
        next_telemetry.measured_rad_s = measured.rad_s;
        next_telemetry.current_raw =
            current_selected ? controller_output.motor_current_raw
                             : std::array<std::int16_t, 4U>{};
        next_telemetry.can = can;

        if (!iteration_overrun &&
            deadline_reached(
                static_cast<std::uint32_t>(tx_time_get()), next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
            next_telemetry.overrun_count = control_overrun_count;
        }
        next_telemetry.state =
            policy.reported_state(controller_output.state);
        next_telemetry.faults = policy.faults();
        publish_telemetry(next_telemetry);

        if (!iteration_overrun &&
            deadline_reached(
                static_cast<std::uint32_t>(tx_time_get()), next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
            next_telemetry.overrun_count = control_overrun_count;
            next_telemetry.state =
                policy.reported_state(controller_output.state);
            next_telemetry.faults = policy.faults();
            publish_telemetry(next_telemetry);
        }
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
    runtime_controller = controller{controller_config_of(config)};
    policy = runtime_policy{false, false, {}};
    remote_subscriber = {};
    publish_remote_snapshot({});
    publish_telemetry({});
    watchdog_sampled = false;
    handler_all_online = false;
    motor_watchdog_phase = {};
    control_loop_count = 0U;
    control_overrun_count = 0U;

    registration_results[0] =
        motor_handler.register_motor(front_left);
    registration_results[1] =
        motor_handler.register_motor(front_right);
    registration_results[2] =
        motor_handler.register_motor(rear_left);
    registration_results[3] =
        motor_handler.register_motor(rear_right);

    any_motor_registered = registration_results[0] ||
                           registration_results[1] ||
                           registration_results[2] ||
                           registration_results[3];
    const bool all_registered = registration_results[0] &&
                                registration_results[1] &&
                                registration_results[2] &&
                                registration_results[3];
    const auto can_baseline =
        bsp::can::snapshot(bsp::can::bus::can1);
    const bool config_valid =
        valid(runtime_configuration) &&
        runtime_configuration.control_period_s == control_period_s;
    policy = runtime_policy{config_valid, all_registered, can_baseline};
    sync_fault_telemetry(can_baseline);

    if (!all_registered)
    {
        send_startup_zero_if_possible(can_baseline);
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

} // namespace vehicle::chassis::runtime
