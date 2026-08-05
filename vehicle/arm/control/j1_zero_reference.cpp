#include "vehicle/arm/control/j1_zero_reference.hpp"

#include <cmath>

namespace vehicle::arm
{

bool j1_zero_reference::capture(float physical_position_rad) noexcept
{
    if (captured_)
    {
        return true;
    }
    if (!std::isfinite(physical_position_rad))
    {
        return false;
    }
    physical_zero_rad_ = physical_position_rad;
    captured_ = true;
    return true;
}

bool j1_zero_reference::captured() const noexcept
{
    return captured_;
}

float j1_zero_reference::logical_position(
    float physical_position_rad) const noexcept
{
    if (!captured_ || !std::isfinite(physical_position_rad))
    {
        return 0.0F;
    }
    const float result = physical_position_rad - physical_zero_rad_;
    return std::isfinite(result) ? result : 0.0F;
}

} // namespace vehicle::arm
