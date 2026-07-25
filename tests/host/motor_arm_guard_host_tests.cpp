#include "dji_motor_arm_guard.hpp"

#include <cstdint>
#include <cstdlib>
#include <limits>

namespace
{

using modules::motor::detail::arm_decision;
using modules::motor::detail::arm_request_guard;
using modules::motor::detail::arm_request_timeout_ticks;

[[noreturn]] void fail() noexcept
{
    std::abort();
}

void require(bool condition) noexcept
{
    if (!condition)
    {
        fail();
    }
}

void test_fresh_request_activates_once() noexcept
{
    arm_request_guard guard{};
    guard.request(100U, 7U);
    require(guard.evaluate(101U, 7U, true) ==
            arm_decision::activate);
    require(guard.evaluate(102U, 7U, true) ==
            arm_decision::none);
}

void test_dropout_cancels_without_stale_replay() noexcept
{
    arm_request_guard guard{};
    guard.request(100U, 7U);
    require(guard.evaluate(101U, 7U, false) ==
            arm_decision::cancel);
    require(guard.evaluate(250U, 8U, true) ==
            arm_decision::none);
}

void test_feedback_generation_change_cancels() noexcept
{
    arm_request_guard guard{};
    guard.request(100U, 7U);
    require(guard.evaluate(101U, 8U, true) ==
            arm_decision::cancel);
}

void test_deadline_is_wrap_safe() noexcept
{
    arm_request_guard expired{};
    expired.request(100U, 7U);
    require(expired.evaluate(
                100U + arm_request_timeout_ticks,
                7U, true) == arm_decision::cancel);

    arm_request_guard wrapped{};
    wrapped.request(
        std::numeric_limits<std::uint32_t>::max() - 5U, 7U);
    require(wrapped.evaluate(3U, 7U, true) ==
            arm_decision::activate);
}

void test_fault_cancel_requires_new_request() noexcept
{
    arm_request_guard guard{};
    guard.request(100U, 7U);
    guard.cancel();
    require(guard.evaluate(101U, 7U, true) ==
            arm_decision::none);
}

} // namespace

int main()
{
    test_fresh_request_activates_once();
    test_dropout_cancels_without_stale_replay();
    test_feedback_generation_change_cancels();
    test_deadline_is_wrap_safe();
    test_fault_cancel_requires_new_request();
    return EXIT_SUCCESS;
}
