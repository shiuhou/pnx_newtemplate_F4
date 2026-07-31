#include "vehicle/chassis/velocity_pi.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vehicle::chassis
{

bool valid(const velocity_pi_config& config) noexcept
{
    return std::isfinite(config.kp) && config.kp >= 0.0F &&
           std::isfinite(config.ki_per_s) && config.ki_per_s >= 0.0F &&
           std::isfinite(config.integral_limit_raw) &&
           config.integral_limit_raw >= 0.0F &&
           (config.kp > 0.0F ||
            (config.ki_per_s > 0.0F &&
             config.integral_limit_raw > 0.0F)) &&
           std::isfinite(config.current_limit_raw) &&
           config.current_limit_raw > 0.0F &&
           config.current_limit_raw <=
               static_cast<float>(std::numeric_limits<std::int16_t>::max());
}

velocity_pi::velocity_pi(velocity_pi_config config) noexcept
    : config_(config), config_valid_(valid(config))
{
}

std::int16_t velocity_pi::update(float target_rad_s,
                                 float measured_rad_s,
                                 float dt_s) noexcept
{
    if (!config_valid_ || !std::isfinite(target_rad_s) ||
        !std::isfinite(measured_rad_s) || !std::isfinite(dt_s) ||
        dt_s <= 0.0F)
    {
        reset();
        return 0;
    }

    const float error = target_rad_s - measured_rad_s;
    if (!std::isfinite(error))
    {
        reset();
        return 0;
    }

    const float candidate_i = std::clamp(
        integral_raw_ + config_.ki_per_s * error * dt_s,
        -config_.integral_limit_raw,
        config_.integral_limit_raw);
    const float unsaturated = config_.kp * error + candidate_i;
    const float saturated = std::clamp(
        unsaturated, -config_.current_limit_raw, config_.current_limit_raw);
    if (unsaturated == saturated || error * unsaturated < 0.0F)
    {
        integral_raw_ = candidate_i;
    }

    if (!std::isfinite(saturated) ||
        saturated < static_cast<float>(std::numeric_limits<std::int16_t>::min()) ||
        saturated > static_cast<float>(std::numeric_limits<std::int16_t>::max()))
    {
        reset();
        return 0;
    }

    return static_cast<std::int16_t>(std::lround(saturated));
}

void velocity_pi::reset() noexcept
{
    integral_raw_ = 0.0F;
}

} // namespace vehicle::chassis
