#include "imu_bmi088.hpp"

#include "bmi088_transport.hpp"
#include "bsp_indicator.hpp"
#include "spi_devices.hpp"
#include "tx_api.h"

#include <cstdlib>

namespace demo::cboard::imu_bmi088
{

volatile telemetry runtime{};

namespace
{

TX_THREAD imu_thread{};
alignas(8) ULONG imu_stack[384]{};
CHAR imu_thread_name[] = "f407 bmi088";

void delay_ms(std::uint32_t milliseconds) noexcept
{
    const ULONG ticks = static_cast<ULONG>(
        (static_cast<std::uint64_t>(milliseconds) *
             TX_TIMER_TICKS_PER_SECOND +
         999U) /
        1000U);
    tx_thread_sleep(ticks == 0U ? 1U : ticks);
}

bool changed(
    const imu::bmi088_raw_sample& previous,
    const imu::bmi088_raw_sample& current) noexcept
{
    constexpr std::int32_t threshold = 4;
    const auto differs = [](std::int16_t first,
                            std::int16_t second) noexcept {
        return std::abs(
                   static_cast<std::int32_t>(first) -
                   static_cast<std::int32_t>(second)) >= threshold;
    };
    return differs(previous.accel_x, current.accel_x) ||
           differs(previous.accel_y, current.accel_y) ||
           differs(previous.accel_z, current.accel_z) ||
           differs(previous.gyro_x, current.gyro_x) ||
           differs(previous.gyro_y, current.gyro_y) ||
           differs(previous.gyro_z, current.gyro_z);
}

void publish(const imu::bmi088_raw_sample& sample) noexcept
{
    runtime.accel_x = sample.accel_x;
    runtime.accel_y = sample.accel_y;
    runtime.accel_z = sample.accel_z;
    runtime.gyro_x = sample.gyro_x;
    runtime.gyro_y = sample.gyro_y;
    runtime.gyro_z = sample.gyro_z;
}

void fail() noexcept
{
    runtime.faulted = 1U;
    (void)bsp::indicator::set(
        bsp::indicator::channel::red, true);
}

void thread_entry(ULONG)
{
    (void)bsp::indicator::init();
    imu::bmi088_transport sensor{
        board::spi::imu_bus,
        board::spi::bmi088_accel,
        board::spi::bmi088_gyro};
    if (sensor.init() != types::status::ok)
    {
        fail();
        return;
    }
    delay_ms(50U);

    imu::bmi088_chip_ids ids{};
    if (sensor.read_chip_ids(ids) != types::status::ok)
    {
        fail();
        return;
    }
    runtime.accel_chip_id = ids.accel;
    runtime.gyro_chip_id = ids.gyro;
    if (!ids.valid() ||
        sensor.configure(delay_ms) != types::status::ok)
    {
        fail();
        return;
    }
    runtime.complete = 1U;

    imu::bmi088_raw_sample previous{};
    bool have_previous = false;
    for (;;)
    {
        imu::bmi088_raw_sample sample{};
        if (sensor.read_raw(sample) != types::status::ok)
        {
            fail();
            return;
        }
        if (have_previous && changed(previous, sample))
        {
            ++runtime.change_count;
        }
        previous = sample;
        have_previous = true;
        publish(sample);
        ++runtime.sample_count;
        ++runtime.heartbeat;
        if ((runtime.sample_count % 50U) == 0U)
        {
            (void)bsp::indicator::toggle(
                bsp::indicator::channel::green);
        }
        tx_thread_sleep(10U);
    }
}

} // namespace

void run() noexcept
{
    if (tx_thread_create(
            &imu_thread, imu_thread_name, thread_entry, 0U,
            imu_stack, sizeof(imu_stack), 10U, 10U,
            TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        (void)bsp::indicator::init();
        fail();
    }
}

} // namespace demo::cboard::imu_bmi088
