#include "vehicle/arm/runtime/arm_config.hpp"

#include <cmath>

namespace vehicle::arm
{

bool valid(const configuration& config) noexcept
{
    return valid(config.j1_position) &&
           std::isfinite(config.j1_manual_position_rate_rad_per_s) &&
           config.j1_manual_position_rate_rad_per_s > 0.0F &&
           std::isfinite(config.control_period_s) &&
           config.control_period_s > 0.0F;
}

} // namespace vehicle::arm
