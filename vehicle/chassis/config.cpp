#include "vehicle/chassis/config.hpp"

#include <cmath>

namespace vehicle::chassis
{

configuration mycar_configuration() noexcept
{
    return {
        {0.0F, 0.0F, 0.0F, 0.0F},
        {0.05F, 0.0F, 0.0F, 0.0F, 1.0F, -1.0F, -1.0F},
        {1.0F, 1.0F, 1.0F, 1.0F},
        {0.0F, 0.0F, 0.0F, 0.0F},
        0.005F,
    };
}

bool valid(const configuration& config) noexcept
{
    const controller_configuration controller_config{
        config.geometry,
        config.manual,
        config.motor_direction,
        config.pi,
    };
    return valid(controller_config) &&
           std::isfinite(config.control_period_s) &&
           config.control_period_s > 0.0F;
}

} // namespace vehicle::chassis
