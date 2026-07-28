#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace demo::cboard::pwm_a2
{

class servo_sequence
{
public:
    constexpr std::uint16_t pulse_us() const noexcept
    {
        return complete() ? 0U : pulses_us_[index_];
    }

    constexpr void advance() noexcept
    {
        if (!complete())
        {
            ++index_;
        }
    }

    constexpr bool complete() const noexcept
    {
        return index_ >= pulses_us_.size();
    }

private:
    static constexpr std::array<std::uint16_t, 5U> pulses_us_{
        1500U, 1450U, 1500U, 1550U, 1500U};
    std::size_t index_ = 0U;
};

} // namespace demo::cboard::pwm_a2
