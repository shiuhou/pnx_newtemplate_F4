#pragma once

#include <cstdint>

namespace demo::cboard::can_m2006
{

class bounded_pulse
{
public:
    static constexpr std::int16_t test_current = 500;
    static constexpr std::uint16_t pulse_cycles = 125U;
    static constexpr std::uint8_t stable_feedback_samples = 10U;

    std::int16_t step(bool feedback_fresh, bool can_healthy) noexcept
    {
        if (state_ == state::complete || state_ == state::fault)
        {
            return 0;
        }
        if (!can_healthy)
        {
            state_ = state::fault;
            return 0;
        }
        if (state_ == state::waiting)
        {
            if (!feedback_fresh)
            {
                stable_feedback_count_ = 0U;
                return 0;
            }
            ++stable_feedback_count_;
            if (stable_feedback_count_ < stable_feedback_samples)
            {
                return 0;
            }
            state_ = state::pulsing;
        }
        else if (!feedback_fresh)
        {
            state_ = state::fault;
            return 0;
        }

        ++pulse_count_;
        if (pulse_count_ >= pulse_cycles)
        {
            state_ = state::complete;
        }
        return test_current;
    }

    bool complete() const noexcept
    {
        return state_ == state::complete;
    }

    bool faulted() const noexcept
    {
        return state_ == state::fault;
    }

    std::uint16_t pulse_count() const noexcept
    {
        return pulse_count_;
    }

private:
    enum class state : std::uint8_t
    {
        waiting = 0,
        pulsing,
        complete,
        fault,
    };

    state state_ = state::waiting;
    std::uint8_t stable_feedback_count_ = 0U;
    std::uint16_t pulse_count_ = 0U;
};

} // namespace demo::cboard::can_m2006
