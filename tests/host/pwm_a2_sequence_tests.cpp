#include "servo_sequence.hpp"

#include <array>
#include <cstdlib>

int main()
{
    using demo::cboard::pwm_a2::servo_sequence;
    constexpr std::array<std::uint16_t, 5U> expected{
        1500U, 1450U, 1500U, 1550U, 1500U};
    servo_sequence sequence{};
    for (const auto pulse : expected)
    {
        if (sequence.complete() || sequence.pulse_us() != pulse)
        {
            std::abort();
        }
        sequence.advance();
    }
    if (!sequence.complete() || sequence.pulse_us() != 0U)
    {
        std::abort();
    }
    return EXIT_SUCCESS;
}
