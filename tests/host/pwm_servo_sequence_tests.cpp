#include "pwm_servo_sequence.hpp"

#include <cstdlib>

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

void test_boot_is_disabled_and_wrong_token_is_rejected() noexcept
{
    cboard::pwm_servo_lab::sequence sequence;
    require(!sequence.output_enabled());
    require(!sequence.arm(0U));
    require(!sequence.output_enabled());
}

void test_exact_token_runs_bounded_sequence_once() noexcept
{
    using cboard::pwm_servo_lab::arm_magic;
    cboard::pwm_servo_lab::sequence sequence;
    require(sequence.arm(arm_magic));

    constexpr std::uint16_t expected[] = {
        1500U, 1450U, 1500U, 1550U, 1500U,
    };
    for (std::uint16_t pulse : expected)
    {
        require(sequence.output_enabled());
        require(sequence.pulse_us() == pulse);
        sequence.advance();
    }

    require(sequence.complete());
    require(sequence.latched());
    require(sequence.output_enabled());
    require(sequence.pulse_us() == 1500U);
    require(!sequence.arm(arm_magic));
}

} // namespace

int main()
{
    test_boot_is_disabled_and_wrong_token_is_rejected();
    test_exact_token_runs_bounded_sequence_once();
    return EXIT_SUCCESS;
}
