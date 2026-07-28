#include "bounded_pulse.hpp"

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

} // namespace

int main()
{
    using demo::cboard::can_m2006::bounded_pulse;

    bounded_pulse no_feedback{};
    for (unsigned i = 0U; i < 100U; ++i)
    {
        require(no_feedback.step(false, true) == 0);
    }

    bounded_pulse pulse{};
    for (unsigned i = 0U; i < bounded_pulse::stable_feedback_samples - 1U;
         ++i)
    {
        require(pulse.step(true, true) == 0);
    }
    unsigned nonzero = 0U;
    for (unsigned i = 0U; i < bounded_pulse::pulse_cycles + 1U; ++i)
    {
        if (pulse.step(true, true) == bounded_pulse::test_current)
        {
            ++nonzero;
        }
    }
    require(nonzero == bounded_pulse::pulse_cycles);
    require(pulse.step(true, true) == 0);
    require(pulse.complete());
    require(pulse.step(true, false) == 0);
    require(pulse.complete());
    require(!pulse.faulted());

    bounded_pulse faulted{};
    for (unsigned i = 0U; i < bounded_pulse::stable_feedback_samples; ++i)
    {
        (void)faulted.step(true, true);
    }
    require(faulted.step(true, false) == 0);
    require(faulted.faulted());
    for (unsigned i = 0U; i < 200U; ++i)
    {
        require(faulted.step(true, true) == 0);
    }
    return EXIT_SUCCESS;
}
