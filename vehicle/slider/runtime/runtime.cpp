#include "vehicle/slider/runtime/runtime.hpp"

#include <config.hpp>
#include <djimotorhandler.hpp>
#include <msg.hpp>
#include <remoter.hpp>
#include <robot_config.hpp>
#include <tx_api.h>

#include <cstddef>
#include <cstdint>

namespace vehicle::slider::runtime
{
namespace
{

constexpr std::uint32_t control_period_ticks = 5U;
constexpr float control_period_s = 0.005F;
constexpr std::uint32_t remote_freshness_ticks = 120U;
constexpr std::uint8_t watchdog_period_cycles = 4U;
constexpr UINT control_priority = 6U;
constexpr std::size_t control_stack_bytes = 1536U;

static_assert(TX_TIMER_TICKS_PER_SECOND == 1000U,
              "Slider runtime requires a one-millisecond ThreadX tick");
static_assert(::config::feature::enable_ps2 &&
                  !::config::feature::enable_dr16,
              "Slider image requires PS2 only");

motors::m2006 slider_motor{robot::motors::slider};
motors::djimotorhandler& motor_handler =
    motors::djimotorhandler::instance();

TX_THREAD control_thread{};
alignas(8) std::uint8_t control_stack[control_stack_bytes]{};
CHAR control_thread_name[] = "m2006 slider";

controller_configuration runtime_configuration{};
controller runtime_controller{runtime_configuration};
msg::subscriber remote_subscriber{};
remoter::state latest_remote{};
telemetry runtime_telemetry{};
std::uint32_t latest_remote_tick{};
std::uint32_t loop_count{};
std::uint8_t watchdog_phase{};
bool remote_seen{};
bool watchdog_sampled{};
bool motor_online{};
bool runtime_start_attempted{};
bool motor_registered{};

limit_state read_limits() noexcept
{
    // Photoelectric sensors are not installed yet. Keeping ready=false makes
    // Circle select automatic mode without permitting motor current.
    return {};
}

bool can_allows_control(bsp::can::state state) noexcept
{
    return state == bsp::can::state::active ||
           state == bsp::can::state::passive;
}

void publish_telemetry(const telemetry& next) noexcept
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    runtime_telemetry = next;
    TX_RESTORE
}

void send_zero() noexcept
{
    slider_motor.relax();
    if (motor_registered)
    {
        motor_handler.send_control();
    }
}

void control_entry(ULONG)
{
    ::remoter::config remote_config{};
    remote_config.ps2.thread_priority = params::remoter::thread_priority;
    remote_config.ps2.receiver_offline_timeout_ticks =
        params::remoter::ps2_offline_timeout_ticks;
    remote_config.ps2.frame_timeout_ticks =
        params::remoter::ps2_frame_timeout_ticks;
    remote_config.ps2.deadzone = params::remoter::ps2_deadzone;
    remote_config.thread_priority = params::remoter::thread_priority + 1U;
    remote_config.offline_timeout_ticks = remote_freshness_ticks;

    if (!::remoter::service::instance().init(remote_config))
    {
        send_zero();
        return;
    }
    remote_subscriber = msg::subscribe<::remoter::state>();
    if (!remote_subscriber.valid())
    {
        send_zero();
        return;
    }

    for (;;)
    {
        remoter::state received{};
        if (msg::read(remote_subscriber, received) == types::status::ok)
        {
            latest_remote = received;
            latest_remote_tick = static_cast<std::uint32_t>(tx_time_get());
            remote_seen = true;
        }

        const std::uint32_t now =
            static_cast<std::uint32_t>(tx_time_get());
        remoter::state control_remote = latest_remote;
        if (!remote_seen ||
            (now - latest_remote_tick) > remote_freshness_ticks)
        {
            control_remote = {};
            control_remote.offline = true;
            control_remote.active_source = remoter::source::none;
        }

        float measured_velocity = 0.0F;
        if (++watchdog_phase >= watchdog_period_cycles)
        {
            watchdog_phase = 0U;
            motor_online = motor_handler.alive_check();
            watchdog_sampled = true;
        }
        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        measured_velocity = slider_motor.get_feedback().velocity;
        TX_RESTORE

        const auto can = bsp::can::snapshot(bsp::can::bus::can1);
        controller_input input{};
        input.remote = control_remote;
        input.limits = read_limits();
        input.measured_velocity_rad_s = measured_velocity;
        input.motor_online = watchdog_sampled && motor_online;
        input.can_healthy = can_allows_control(can.bus_state);
        input.dt_s = control_period_s;
        const auto output = runtime_controller.update(input);

        if (output.outputs_enabled)
        {
            slider_motor.set_current(output.current_raw);
        }
        else
        {
            slider_motor.relax();
        }
        motor_handler.send_control();

        telemetry next{};
        next.mode = output.mode;
        next.direction = output.direction;
        next.outputs_enabled = output.outputs_enabled;
        next.watchdog_sampled = watchdog_sampled;
        next.motor_online = motor_online;
        next.limits_ready = input.limits.ready;
        next.right_x = control_remote.right_x;
        next.target_velocity_rad_s = output.target_velocity_rad_s;
        next.measured_velocity_rad_s = measured_velocity;
        next.current_raw = output.current_raw;
        next.loop_count = ++loop_count;
        next.can = can;
        publish_telemetry(next);
        tx_thread_sleep(control_period_ticks);
    }
}

} // namespace

void start(const controller_configuration& config) noexcept
{
    if (runtime_start_attempted)
    {
        return;
    }
    runtime_start_attempted = true;
    runtime_configuration = config;
    runtime_controller = controller{config};
    publish_telemetry({});

    motor_registered = motor_handler.register_motor(slider_motor);
    if (!valid(config) || !motor_registered)
    {
        send_zero();
        return;
    }
    send_zero();
    if (tx_thread_create(
            &control_thread, control_thread_name, control_entry, 0U,
            control_stack, sizeof(control_stack), control_priority,
            control_priority, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS)
    {
        send_zero();
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

} // namespace vehicle::slider::runtime
