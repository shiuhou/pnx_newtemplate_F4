#include "can_m2006.hpp"

#include "bounded_pulse.hpp"
#include "bsp_can.hpp"
#include "bsp_indicator.hpp"
#include "djimotorhandler.hpp"
#include "djimotors.hpp"
#include "tx_api.h"

namespace demo::cboard::can_m2006
{

volatile telemetry runtime{};

namespace
{

TX_THREAD can_thread{};
alignas(8) ULONG can_stack[512]{};
CHAR can_thread_name[] = "f407 can m2006";

motors::config motor_config() noexcept
{
    motors::config config{};
    config.can_bus = bsp::can::bus::can1;
    config.can_type = bsp::can::bus_type::classic;
    config.can_id = 0x203U;
    return config;
}

motors::m2006 motor{motor_config()};

void update_runtime(const bsp::can::telemetry& can_state,
                    const bounded_pulse& pulse,
                    std::int16_t current) noexcept
{
    ++runtime.heartbeat;
    runtime.rx_count = can_state.rx_count;
    runtime.tx_count = can_state.tx_count;
    runtime.last_id = can_state.last_id;
    runtime.error_count = can_state.error_count;
    runtime.drop_count = can_state.drop_count;
    runtime.fault_epoch = can_state.fault_epoch;
    runtime.pulse_count = pulse.pulse_count();
    runtime.commanded_current = current;
    runtime.complete = pulse.complete() ? 1U : 0U;
    runtime.faulted = pulse.faulted() ? 1U : 0U;
}

void thread_entry(ULONG)
{
    const bool indicator_ready =
        bsp::indicator::init() == types::status::ok;
    auto& handler = motors::djimotorhandler::instance();
    const bool can_ready = handler.register_motor(motor);
    if (!indicator_ready || !can_ready)
    {
        runtime.faulted = 1U;
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
        for (;;)
        {
            ++runtime.heartbeat;
            tx_thread_sleep(100U);
        }
    }

    bounded_pulse pulse{};
    std::uint32_t previous_rx_count = 0U;
    for (;;)
    {
        const bsp::can::telemetry before =
            bsp::can::snapshot(bsp::can::bus::can1);
        const bool feedback_fresh =
            before.rx_count != previous_rx_count &&
            before.last_id == 0x203U;
        previous_rx_count = before.rx_count;
        const bool can_healthy =
            before.bus_state == bsp::can::state::active &&
            before.error_count == 0U &&
            before.drop_count == 0U &&
            before.fault_epoch == 0U;

        const std::int16_t current =
            pulse.step(feedback_fresh, can_healthy);
        motor.set_current(current);
        handler.send_control();

        const bsp::can::telemetry after =
            bsp::can::snapshot(bsp::can::bus::can1);
        update_runtime(after, pulse, current);

        if (pulse.faulted())
        {
            motor.relax();
            handler.send_control();
            (void)bsp::indicator::set(
                bsp::indicator::channel::red, true);
        }
        else if ((runtime.heartbeat % 250U) == 0U)
        {
            (void)bsp::indicator::toggle(
                bsp::indicator::channel::green);
        }
        tx_thread_sleep(2U);
    }
}

} // namespace

void run() noexcept
{
    if (tx_thread_create(
            &can_thread, can_thread_name, thread_entry, 0U,
            can_stack, sizeof(can_stack), 10U, 10U,
            TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        runtime.faulted = 1U;
        (void)bsp::indicator::init();
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
    }
}

} // namespace demo::cboard::can_m2006
