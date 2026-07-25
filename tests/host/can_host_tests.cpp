#include "bsp_can.hpp"
#include "fake_can_backend.hpp"

#include <array>
#include <cstdlib>
#include <string_view>

namespace
{

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

void init_can() noexcept
{
    host_test::fake_can::reset();
    require(bsp::can::init(
                bsp::can::bus::can1, bsp::can::bus_type::classic) ==
            types::status::ok);
}

void inject_bus_off() noexcept
{
    host_test::fake_can::set_tick(10U);
    bsp::can::detail::error_from_isr(
        bsp::can::bus::can1, bsp::can::state::bus_off, 10U);
}

void test_recovery_success() noexcept
{
    init_can();
    inject_bus_off();
    const bsp::can::telemetry faulted =
        bsp::can::snapshot(bsp::can::bus::can1);
    require(faulted.bus_state == bsp::can::state::bus_off);
    require(faulted.bus_off_count == 1U);
    require(faulted.error_count == 1U);
    require(faulted.fault_epoch == 1U);

    require(bsp::can::recover(bsp::can::bus::can1) ==
            types::status::ok);
    const bsp::can::telemetry recovered =
        bsp::can::snapshot(bsp::can::bus::can1);
    require(recovered.bus_state == bsp::can::state::active);
    require(recovered.error_count == faulted.error_count);
    require(recovered.fault_epoch == faulted.fault_epoch);
    require(host_test::fake_can::recover_calls() == 1U);
    require(host_test::fake_can::observed_recovering_state());
}

void test_recovery_failure() noexcept
{
    init_can();
    inject_bus_off();
    host_test::fake_can::set_recover_status(types::status::error);

    require(bsp::can::recover(bsp::can::bus::can1) ==
            types::status::error);
    const bsp::can::telemetry failed =
        bsp::can::snapshot(bsp::can::bus::can1);
    require(failed.bus_state == bsp::can::state::fault);
    require(failed.error_count == 2U);
    require(failed.fault_epoch == 2U);
    require(host_test::fake_can::recover_calls() == 1U);
}

void test_plain_transmit_recovers_in_thread_context() noexcept
{
    init_can();
    inject_bus_off();
    constexpr std::array<std::uint8_t, 8> zero{};

    require(bsp::can::transmit(
                bsp::can::bus::can1, 0x200U,
                zero.data(), zero.size()) == types::status::ok);
    require(host_test::fake_can::recover_calls() == 1U);
    require(host_test::fake_can::transmit_calls() == 1U);
    require(bsp::can::snapshot(
                bsp::can::bus::can1).bus_state ==
            bsp::can::state::active);
}

void test_guarded_transmit_never_auto_recovers() noexcept
{
    init_can();
    const bsp::can::telemetry expected =
        bsp::can::snapshot(bsp::can::bus::can1);
    inject_bus_off();
    constexpr std::array<std::uint8_t, 8> command{};

    require(bsp::can::transmit_if_healthy(
                bsp::can::bus::can1, 0x200U,
                command.data(), command.size(),
                expected.error_count, expected.drop_count,
                expected.fault_epoch) == types::status::error);
    require(host_test::fake_can::recover_calls() == 0U);
    require(host_test::fake_can::transmit_calls() == 0U);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        return EXIT_FAILURE;
    }
    const std::string_view scenario{argv[1]};
    if (scenario == "recovery_success")
    {
        test_recovery_success();
    }
    else if (scenario == "recovery_failure")
    {
        test_recovery_failure();
    }
    else if (scenario == "plain_transmit_recovery")
    {
        test_plain_transmit_recovers_in_thread_context();
    }
    else if (scenario == "guarded_transmit_no_recovery")
    {
        test_guarded_transmit_never_auto_recovers();
    }
    else
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
