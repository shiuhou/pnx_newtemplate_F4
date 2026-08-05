#include "vehicle/arm/runtime/servo_pwm_output.hpp"

namespace vehicle::arm
{

servo_pwm_output::servo_pwm_output(servo_pwm_output_config config) noexcept
    : config_(config),
      config_valid_(config.channel != bsp::pwm::none &&
                    config.period_us > 0U && config.period_us <= 65536U)
{
}

bool servo_pwm_output::update(bool enabled,
                              std::uint32_t pulse_us) noexcept
{
    if (!enabled)
    {
        if (enabled_ &&
            bsp::pwm::stop(config_.channel) != types::status::ok)
        {
            fail_closed();
            return false;
        }
        enabled_ = false;
        pulse_us_ = 0U;
        return true;
    }

    if (!config_valid_ || pulse_us > config_.period_us)
    {
        fail_closed();
        return false;
    }

    if (!enabled_)
    {
        if (bsp::pwm::set_period_us(config_.channel, config_.period_us) !=
                types::status::ok ||
            bsp::pwm::set_pulse_us(config_.channel, pulse_us) !=
                types::status::ok ||
            bsp::pwm::start(config_.channel) != types::status::ok)
        {
            fail_closed();
            return false;
        }
        enabled_ = true;
    }
    else if (bsp::pwm::set_pulse_us(config_.channel, pulse_us) !=
             types::status::ok)
    {
        fail_closed();
        return false;
    }

    pulse_us_ = pulse_us;
    return true;
}

bool servo_pwm_output::enabled() const noexcept
{
    return enabled_;
}

std::uint32_t servo_pwm_output::pulse_us() const noexcept
{
    return pulse_us_;
}

void servo_pwm_output::fail_closed() noexcept
{
    if (config_.channel != bsp::pwm::none)
    {
        (void)bsp::pwm::stop(config_.channel);
    }
    enabled_ = false;
    pulse_us_ = 0U;
}

} // namespace vehicle::arm
