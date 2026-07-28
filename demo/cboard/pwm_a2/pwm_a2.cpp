#include "pwm_a2.hpp"

#include "bsp_indicator.hpp"
#include "bsp_pwm.hpp"
#include "pwm_channels.hpp"
#include "servo_sequence.hpp"
#include "tx_api.h"

namespace demo::cboard::pwm_a2
{

volatile telemetry runtime{};

namespace
{

TX_THREAD servo_thread{};
alignas(8) ULONG servo_stack[256]{};
CHAR servo_thread_name[] = "f407 pwm a2";

void fail() noexcept
{
    runtime.faulted = 1U;
    runtime.output_enabled = 0U;
    runtime.pulse_us = 0U;
    (void)bsp::pwm::stop(board::pwm::servo_c2);
    (void)bsp::indicator::set(bsp::indicator::channel::red, true);
}

void thread_entry(ULONG)
{
    if (bsp::indicator::init() != types::status::ok ||
        bsp::pwm::set_period_us(
            board::pwm::servo_c2, 20000U) != types::status::ok)
    {
        fail();
        return;
    }

    servo_sequence sequence{};
    while (!sequence.complete())
    {
        const std::uint32_t pulse = sequence.pulse_us();
        if (bsp::pwm::set_pulse_us(
                board::pwm::servo_c2, pulse) != types::status::ok)
        {
            fail();
            return;
        }
        if (runtime.output_enabled == 0U)
        {
            if (bsp::pwm::start(
                    board::pwm::servo_c2) != types::status::ok)
            {
                fail();
                return;
            }
            runtime.output_enabled = 1U;
        }

        runtime.pulse_us = pulse;
        ++runtime.step_count;
        ++runtime.heartbeat;
        (void)bsp::indicator::toggle(
            bsp::indicator::channel::green);
        tx_thread_sleep(500U);
        sequence.advance();
    }

    if (bsp::pwm::set_pulse_us(
            board::pwm::servo_c2, 1500U) != types::status::ok)
    {
        fail();
        return;
    }
    tx_thread_sleep(250U);
    if (bsp::pwm::stop(
            board::pwm::servo_c2) != types::status::ok)
    {
        fail();
        return;
    }
    runtime.pulse_us = 0U;
    runtime.output_enabled = 0U;
    runtime.complete = 1U;
    (void)bsp::indicator::set(
        bsp::indicator::channel::green, true);
}

} // namespace

void run() noexcept
{
    if (tx_thread_create(
            &servo_thread, servo_thread_name, thread_entry, 0U,
            servo_stack, sizeof(servo_stack), 10U, 10U,
            TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        (void)bsp::indicator::init();
        fail();
    }
}

} // namespace demo::cboard::pwm_a2
