#pragma once

#include "bsp_pwm.hpp"

#include <cstdint>

namespace vehicle::arm
{

struct servo_pwm_output_config {
    bsp::pwm::channel channel{bsp::pwm::none};
    std::uint32_t period_us{};
};

class servo_pwm_output {
public:
    explicit servo_pwm_output(servo_pwm_output_config config) noexcept;

    bool update(bool enabled, std::uint32_t pulse_us) noexcept;
    bool enabled() const noexcept;
    std::uint32_t pulse_us() const noexcept;

private:
    void fail_closed() noexcept;

    servo_pwm_output_config config_{};
    std::uint32_t pulse_us_{};
    bool config_valid_{};
    bool enabled_{};
};

} // namespace vehicle::arm
