#pragma once

#include <cstdint>

namespace demo::cboard::imu_bmi088
{

struct telemetry
{
    std::uint32_t heartbeat = 0U;
    std::uint32_t sample_count = 0U;
    std::uint32_t change_count = 0U;
    std::uint32_t accel_chip_id = 0U;
    std::uint32_t gyro_chip_id = 0U;
    std::int32_t accel_x = 0;
    std::int32_t accel_y = 0;
    std::int32_t accel_z = 0;
    std::int32_t gyro_x = 0;
    std::int32_t gyro_y = 0;
    std::int32_t gyro_z = 0;
    std::uint32_t complete = 0U;
    std::uint32_t faulted = 0U;
};

extern volatile telemetry runtime;

void run() noexcept;

} // namespace demo::cboard::imu_bmi088
