#pragma once

#include <cstdint>
#include <cstdlib>

namespace cboard::ist8310_lab
{

constexpr std::uint8_t i2c_address_7bit = 0x0EU;
constexpr std::uint8_t who_am_i_value = 0x10U;

struct raw_sample
{
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
};

constexpr std::int16_t decode_i16(
    std::uint8_t low, std::uint8_t high) noexcept
{
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(low) |
        (static_cast<std::uint16_t>(high) << 8U));
}

constexpr bool identity_valid(std::uint8_t id) noexcept
{
    return id == who_am_i_value;
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
    return changed(first.x, next.x) ||
           changed(first.y, next.y) ||
           changed(first.z, next.z);
}

} // namespace cboard::ist8310_lab
