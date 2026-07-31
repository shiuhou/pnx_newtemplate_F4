#pragma once

#include <cstdint>

namespace vehicle::chassis
{

struct velocity_pi_config {
    float kp{};
    float ki_per_s{};
    float integral_limit_raw{};
    float current_limit_raw{};
};

bool valid(const velocity_pi_config& config) noexcept;

class velocity_pi {
public:
    explicit velocity_pi(velocity_pi_config config) noexcept;

    std::int16_t update(float target_rad_s,
                        float measured_rad_s,
                        float dt_s) noexcept;
    void reset() noexcept;

private:
    velocity_pi_config config_{};
    float integral_raw_{};
    bool config_valid_{};
};

} // namespace vehicle::chassis
