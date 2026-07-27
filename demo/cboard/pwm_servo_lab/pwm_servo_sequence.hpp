#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cboard::pwm_servo_lab
{

inline constexpr std::uint32_t arm_magic = 0x50574D32U;

class sequence
{
  public:
    bool arm(std::uint32_t token) noexcept
    {
        if (token != arm_magic || active_ || latched_)
        {
            return false;
        }
        active_ = true;
        index_ = 0U;
        return true;
    }

    void advance() noexcept
    {
        if (!active_)
        {
            return;
        }
        if (index_ + 1U < pulses_us.size())
        {
            ++index_;
            return;
        }
        active_ = false;
        latched_ = true;
    }

    bool output_enabled() const noexcept
    {
        return active_ || latched_;
    }

    bool complete() const noexcept
    {
        return latched_;
    }

    bool latched() const noexcept
    {
        return latched_;
    }

    std::uint16_t pulse_us() const noexcept
    {
        return output_enabled() ? pulses_us[index_] : 0U;
    }

  private:
    inline static constexpr std::array<std::uint16_t, 5U> pulses_us{
        1500U, 1450U, 1500U, 1550U, 1500U,
    };

    std::size_t index_ = 0U;
    bool active_ = false;
    bool latched_ = false;
};

} // namespace cboard::pwm_servo_lab
