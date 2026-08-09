#include "vehicle/combined/control/mode_router.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::combined::control_mode;
using vehicle::combined::mode_router;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

remoter::state online_centered_dr16() noexcept
{
    remoter::state remote{};
    remote.offline = false;
    remote.active_source = remoter::source::dr16;
    remote.left_sw = remoter::sw_state::mid;
    remote.right_sw = remoter::sw_state::up;
    return remote;
}

void test_switch_maps_modes_and_offline_is_neutral() noexcept
{
    mode_router router{};
    auto remote = online_centered_dr16();

    remote.left_sw = remoter::sw_state::up;
    require(router.update(remote, 0.05F, 0.05F).mode ==
            control_mode::chassis);

    remote.left_sw = remoter::sw_state::low;
    require(router.update(remote, 0.05F, 0.05F).mode ==
            control_mode::arm);

    remote.left_sw = remoter::sw_state::mid;
    require(router.update(remote, 0.05F, 0.05F).mode ==
            control_mode::neutral);

    remote.left_sw = remoter::sw_state::up;
    remote.offline = true;
    const auto offline = router.update(remote, 0.05F, 0.05F);
    require(offline.mode == control_mode::neutral);
    require(!offline.remote_online);
    require(!offline.chassis_ready);
    require(offline.chassis_remote.right_sw == remoter::sw_state::mid);
}

void test_centered_held_enable_allows_chassis_entry() noexcept
{
    mode_router router{};
    auto remote = online_centered_dr16();
    remote.left_sw = remoter::sw_state::up;

    const auto output = router.update(remote, 0.05F, 0.05F);
    require(output.mode == control_mode::chassis);
    require(output.chassis_ready);
    require(output.chassis_remote.right_sw == remoter::sw_state::up);
}

void test_off_center_entry_waits_for_center_without_dropping_enable() noexcept
{
    mode_router router{};
    auto remote = online_centered_dr16();
    remote.left_sw = remoter::sw_state::low;
    (void)router.update(remote, 0.05F, 0.05F);

    remote.left_sw = remoter::sw_state::up;
    remote.left_x = 0.40F;
    auto output = router.update(remote, 0.05F, 0.05F);
    require(!output.chassis_ready);
    require(output.chassis_remote.right_sw == remoter::sw_state::mid);
    require(output.chassis_remote.left_x == 0.0F);

    remote.left_x = 0.0F;
    output = router.update(remote, 0.05F, 0.05F);
    require(output.chassis_ready);
    require(output.chassis_remote.right_sw == remoter::sw_state::up);
}

void test_release_clears_ready_and_rearm_requires_center() noexcept
{
    mode_router router{};
    auto remote = online_centered_dr16();
    remote.left_sw = remoter::sw_state::up;
    require(router.update(remote, 0.05F, 0.05F).chassis_ready);

    remote.right_sw = remoter::sw_state::mid;
    remote.left_y = 0.50F;
    auto output = router.update(remote, 0.05F, 0.05F);
    require(!output.chassis_ready);
    require(output.chassis_remote.right_sw == remoter::sw_state::mid);

    remote.right_sw = remoter::sw_state::up;
    output = router.update(remote, 0.05F, 0.05F);
    require(!output.chassis_ready);

    remote.left_y = 0.0F;
    output = router.update(remote, 0.05F, 0.05F);
    require(output.chassis_ready);
}

void test_mode_exit_and_invalid_input_clear_ready() noexcept
{
    mode_router router{};
    auto remote = online_centered_dr16();
    remote.left_sw = remoter::sw_state::up;
    require(router.update(remote, 0.05F, 0.05F).chassis_ready);

    remote.left_sw = remoter::sw_state::low;
    remote.right_y = 0.04F;
    auto output = router.update(remote, 0.05F, 0.05F);
    require(output.mode == control_mode::arm);
    require(output.arm_axes_centered);
    require(!output.chassis_ready);

    remote.right_y = 0.20F;
    output = router.update(remote, 0.05F, 0.05F);
    require(!output.arm_axes_centered);

    remote.left_sw = remoter::sw_state::up;
    remote.right_y = 0.0F;
    remote.left_x = std::numeric_limits<float>::quiet_NaN();
    output = router.update(remote, 0.05F, 0.05F);
    require(!output.chassis_ready);

    router.reset();
    remote.left_x = 0.0F;
    output = router.update(remote, -1.0F, 0.05F);
    require(output.mode == control_mode::neutral);
    require(!output.remote_online);
}

} // namespace

int main()
{
    test_switch_maps_modes_and_offline_is_neutral();
    test_centered_held_enable_allows_chassis_entry();
    test_off_center_entry_waits_for_center_without_dropping_enable();
    test_release_clears_ready_and_rearm_requires_center();
    test_mode_exit_and_invalid_input_clear_ready();
    return EXIT_SUCCESS;
}
