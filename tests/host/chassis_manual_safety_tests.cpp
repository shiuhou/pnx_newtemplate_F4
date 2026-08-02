#include "vehicle/chassis/control/manual_control.hpp"
#include "vehicle/chassis/control/safety_gate.hpp"

// 可執行規格：DR16 死區映射，以及「先釋放、後上撥」解鎖與故障鎖定規則。

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::chassis::body_velocity;
using vehicle::chassis::manual_input;
using vehicle::chassis::manual_limits;
using vehicle::chassis::map_manual;
using vehicle::chassis::safety_gate;
using vehicle::chassis::safety_input;
using vehicle::chassis::safety_state;

constexpr manual_limits limits{0.05F, 1.0F, 1.0F, 2.0F, 1.0F, -1.0F, -1.0F};

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool near(float actual, float expected) noexcept
{
    return std::fabs(actual - expected) < 0.0001F;
}

void require_zero(const body_velocity& velocity) noexcept
{
    require(near(velocity.vx_mps, 0.0F));
    require(near(velocity.vy_mps, 0.0F));
    require(near(velocity.yaw_rad_s, 0.0F));
}

constexpr safety_input healthy_disarmed{true, false, true, true, true};
constexpr safety_input healthy_armed_request{true, true, true, true, true};

void arm(safety_gate& gate) noexcept
{
    require(gate.update(healthy_disarmed) == safety_state::disabled);
    require(!gate.output_enabled());
    require(gate.update(healthy_armed_request) == safety_state::armed);
    require(gate.output_enabled());
}

void test_manual_mapping() noexcept
{
    manual_input named_input{};
    named_input.online = true;
    named_input.arm_switches_up = true;
    named_input.left_x = -0.25F;
    named_input.left_y = 0.50F;
    named_input.right_x = 0.75F;
    require(named_input.online);
    require(named_input.arm_switches_up);
    require(near(named_input.left_x, -0.25F));
    require(near(named_input.left_y, 0.50F));
    require(near(named_input.right_x, 0.75F));
    const auto named_mapped = map_manual(named_input, limits);
    require(named_mapped.vx_mps > 0.0F);
    require(named_mapped.vy_mps > 0.0F);
    require(named_mapped.yaw_rad_s < 0.0F);

    const auto deadbanded = map_manual({true, true, 0.02F, 0.02F, 0.02F}, limits);
    require_zero(deadbanded);

    constexpr float just_over_deadband = (0.06F - 0.05F) / (1.0F - 0.05F);
    const auto rescaled = map_manual({true, true, -0.06F, 0.06F, 0.06F}, limits);
    require(near(rescaled.vx_mps, just_over_deadband));
    require(near(rescaled.vy_mps, just_over_deadband));
    require(near(rescaled.yaw_rad_s, -2.0F * just_over_deadband));

    require(near(map_manual({true, true, 0.0F, 1.0F, 0.0F}, limits).vx_mps, 1.0F));
    require(near(map_manual({true, true, 1.0F, 0.0F, 0.0F}, limits).vy_mps, -1.0F));
    require(near(map_manual({true, true, 0.0F, 0.0F, 1.0F}, limits).yaw_rad_s, -2.0F));

    const auto offline = map_manual({false, true, 1.0F, 1.0F, 1.0F}, limits);
    require_zero(offline);
    require_zero(map_manual({true, false, 1.0F, 1.0F, 1.0F}, limits));

    const auto clamped = map_manual({true, true, 2.0F, -2.0F, 2.0F}, limits);
    require(near(clamped.vx_mps, -1.0F));
    require(near(clamped.vy_mps, -1.0F));
    require(near(clamped.yaw_rad_s, -2.0F));
}

void test_manual_rejects_malformed_data() noexcept
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();
    require_zero(map_manual({true, true, nan, 0.0F, 0.0F}, limits));
    require_zero(map_manual({true, true, 0.0F, infinity, 0.0F}, limits));
    require_zero(map_manual({true, true, 0.0F, 0.0F, 0.0F},
                            {nan, 1.0F, 1.0F, 2.0F, 1.0F, -1.0F, -1.0F}));
    require_zero(map_manual({true, true, 0.0F, 0.0F, 0.0F},
                            {0.05F, 1.0F, infinity, 2.0F, 1.0F, -1.0F, -1.0F}));
    require_zero(map_manual({true, true, 0.0F, 0.0F, 0.0F},
                            {1.0F, 1.0F, 1.0F, 2.0F, 1.0F, -1.0F, -1.0F}));
}

void test_safety_requires_release_then_rise() noexcept
{
    safety_gate gate;
    require(gate.update(healthy_armed_request) == safety_state::disabled);
    require(!gate.output_enabled());
    require(gate.update(healthy_armed_request) == safety_state::disabled);
    require(!gate.output_enabled());
    arm(gate);
}

void test_safety_ignores_offline_switch_history() noexcept
{
    safety_gate gate;

    require(gate.update({false, false, true, true, true}) ==
            safety_state::waiting_remote);
    require(gate.update(healthy_armed_request) == safety_state::disabled);
    require(!gate.output_enabled());

    require(gate.update({false, false, true, true, true}) ==
            safety_state::waiting_remote);
    require(gate.update(healthy_armed_request) == safety_state::disabled);
    require(!gate.output_enabled());

    require(gate.update(healthy_disarmed) == safety_state::disabled);
    require(gate.update(healthy_armed_request) == safety_state::armed);
    require(gate.output_enabled());
}

void test_safety_waiting_states() noexcept
{
    safety_gate gate;
    require(gate.update({false, false, true, true, true}) ==
            safety_state::waiting_remote);
    require(gate.update({true, false, false, true, true}) ==
            safety_state::waiting_motors);
    require(gate.update({true, false, true, false, true}) ==
            safety_state::waiting_motors);
    require(gate.update({true, false, true, true, false}) == safety_state::disabled);
    require(!gate.output_enabled());
}

void test_safety_disarms_in_same_step() noexcept
{
    safety_gate gate;
    arm(gate);
    require(gate.update({true, false, true, true, true}) == safety_state::disabled);
    require(!gate.output_enabled());

    safety_gate health_loss_gate;
    arm(health_loss_gate);
    require(health_loss_gate.update({false, false, true, true, true}) ==
            safety_state::fault_latched);
    require(!health_loss_gate.output_enabled());
    health_loss_gate.reset();
    require(health_loss_gate.update(healthy_disarmed) ==
            safety_state::fault_latched);
    health_loss_gate.reset();
    require(health_loss_gate.update(healthy_disarmed) ==
            safety_state::disabled);
    require(!health_loss_gate.output_enabled());
}

void test_safety_latches_each_armed_health_loss() noexcept
{
    for (const safety_input fault : {
             safety_input{false, true, true, true, true},
             safety_input{true, true, false, true, true},
             safety_input{true, true, true, false, true},
             safety_input{true, true, true, true, false},
         })
    {
        safety_gate gate;
        arm(gate);
        require(gate.update(fault) == safety_state::fault_latched);
        require(!gate.output_enabled());
        require(gate.update(healthy_armed_request) == safety_state::fault_latched);
        require(!gate.output_enabled());
    }
}

void test_safety_consumes_unhealthy_arm_edge() noexcept
{
    safety_gate gate;
    require(gate.update(healthy_disarmed) == safety_state::disabled);
    require(gate.update({true, true, false, true, true}) ==
            safety_state::waiting_motors);
    require(gate.update(healthy_armed_request) == safety_state::disabled);
    require(!gate.output_enabled());
    require(gate.update(healthy_disarmed) == safety_state::disabled);
    require(gate.update(healthy_armed_request) == safety_state::armed);
    require(gate.output_enabled());
}

void test_safety_reset_requires_release_after_fault() noexcept
{
    safety_gate gate;
    arm(gate);
    require(gate.update({false, true, true, true, true}) == safety_state::fault_latched);
    gate.reset();
    require(gate.update(healthy_armed_request) == safety_state::fault_latched);
    require(!gate.output_enabled());

    require(gate.update(healthy_disarmed) == safety_state::fault_latched);
    gate.reset();
    require(gate.update(healthy_disarmed) == safety_state::disabled);
    require(!gate.output_enabled());
    require(gate.update(healthy_armed_request) == safety_state::armed);
    require(gate.output_enabled());
}

void test_safety_offline_low_cannot_release_fault() noexcept
{
    safety_gate gate;
    arm(gate);
    require(gate.update({false, true, true, true, true}) ==
            safety_state::fault_latched);

    require(gate.update({false, false, true, true, true}) ==
            safety_state::fault_latched);
    gate.reset();
    require(gate.update(healthy_armed_request) ==
            safety_state::fault_latched);
    require(!gate.output_enabled());

    require(gate.update(healthy_disarmed) ==
            safety_state::fault_latched);
    gate.reset();
    require(gate.update(healthy_armed_request) == safety_state::armed);
    require(gate.output_enabled());
}

} // namespace

int main()
{
    test_manual_mapping();
    test_manual_rejects_malformed_data();
    test_safety_requires_release_then_rise();
    test_safety_ignores_offline_switch_history();
    test_safety_waiting_states();
    test_safety_disarms_in_same_step();
    test_safety_latches_each_armed_health_loss();
    test_safety_consumes_unhealthy_arm_edge();
    test_safety_reset_requires_release_after_fault();
    test_safety_offline_low_cannot_release_fault();
    return EXIT_SUCCESS;
}
