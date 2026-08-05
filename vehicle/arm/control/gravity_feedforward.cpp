#include "vehicle/arm/control/gravity_feedforward.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vehicle::arm
{
namespace
{

constexpr float c610_max_current_raw = 10000.0F;

} // namespace

float gravity_current_raw(float position_rad,
                          float amplitude_raw,
                          float phase_rad,
                          float bias_raw) noexcept
{
    if (!std::isfinite(position_rad) || !std::isfinite(amplitude_raw) ||
        !std::isfinite(phase_rad) || !std::isfinite(bias_raw))
    {
        return 0.0F;
    }
    const float result =
        amplitude_raw * std::cos(position_rad + phase_rad) + bias_raw;
    return std::isfinite(result) ? result : 0.0F;
}

std::int16_t combine_current_raw(std::int16_t feedback_raw,
                                 float feedforward_raw,
                                 float current_limit_raw) noexcept
{
    if (!std::isfinite(feedforward_raw) ||
        !std::isfinite(current_limit_raw) || current_limit_raw <= 0.0F ||
        current_limit_raw > c610_max_current_raw)
    {
        return 0;
    }

    const float combined =
        static_cast<float>(feedback_raw) + feedforward_raw;
    if (!std::isfinite(combined))
    {
        return 0;
    }
    const float limited = std::clamp(
        combined, -current_limit_raw, current_limit_raw);
    if (limited < static_cast<float>(std::numeric_limits<std::int16_t>::min()) ||
        limited > static_cast<float>(std::numeric_limits<std::int16_t>::max()))
    {
        return 0;
    }
    return static_cast<std::int16_t>(std::lround(limited));
}

} // namespace vehicle::arm
