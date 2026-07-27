#pragma once

#include <cstdint>
#include <cstdlib>

namespace cboard::bmi088_lab
{

constexpr std::uint8_t accel_chip_id = 0x1EU;
constexpr std::uint8_t gyro_chip_id = 0x0FU;

struct raw_sample
{
    std::int16_t ax;
    std::int16_t ay;
    std::int16_t az;
    std::int16_t gx;
    std::int16_t gy;
    std::int16_t gz;
};

constexpr std::int16_t decode_i16(
    std::uint8_t low, std::uint8_t high) noexcept
{
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(low) |
        (static_cast<std::uint16_t>(high) << 8U));
}

constexpr bool chip_ids_valid(
    std::uint8_t accel, std::uint8_t gyro) noexcept
{
    return accel == accel_chip_id && gyro == gyro_chip_id;
}

inline bool sample_changed(
    const raw_sample& first, const raw_sample& next,
    std::int32_t threshold) noexcept
{
    const auto changed = [threshold](std::int16_t a, std::int16_t b) {
        return std::abs(
                   static_cast<std::int32_t>(a) -
                   static_cast<std::int32_t>(b)) >= threshold;
    };
    return changed(first.ax, next.ax) ||
           changed(first.ay, next.ay) ||
           changed(first.az, next.az) ||
           changed(first.gx, next.gx) ||
           changed(first.gy, next.gy) ||
           changed(first.gz, next.gz);
}

} // namespace cboard::bmi088_lab
