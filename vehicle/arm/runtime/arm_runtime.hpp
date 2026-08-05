#pragma once

#include "vehicle/arm/control/arm_safety_gate.hpp"
#include "vehicle/arm/runtime/arm_config.hpp"

#include <bsp_can.hpp>
#include <types.hpp>

#include <cstdint>

namespace vehicle::arm
{

enum class control_mode : std::uint8_t {
    neutral,
    chassis,
    arm,
};

enum class runtime_fault : std::uint32_t {
    none = 0U,
    invalid_config = 1U << 0U,
    registration_failed = 1U << 1U,
    remoter_init_failed = 1U << 2U,
    subscribe_failed = 1U << 3U,
    thread_create_failed = 1U << 4U,
    pwm_failed = 1U << 5U,
    overrun = 1U << 6U,
};

// Arm mode uses right_y for J1, left_y for J2, right_x for J3, and left_x
// for the J4 gripper.
struct manual_input {
    bool online{};
    control_mode mode{control_mode::neutral};
    bool arm_mode_selected{};
    bool right_switch_up{};
    bool enable_switches_up{};
    float j1_axis{};
    float j2_axis{};
    float j3_axis{};
    float gripper_axis{};
};

struct runtime_policy_input {
    remoter::state remote{};
    bsp::can::telemetry can{};
    bool watchdog_sampled{};
    bool j1_online{};
    bool manual_axes_centered{};
    bool overrun{};
};

struct runtime_policy_output {
    manual_input manual{};
    arm_safety_input safety{};
    bool force_zero{true};
    bool fault_latched{};
};

struct j1_hold_input {
    bool arm_control_enabled{};
    bool remote_online{};
    bool right_switch_up{};
    bool j1_online{};
    bool can_healthy{};
    bool config_valid{};
    bool fault_latched{};
};

class j1_hold_gate {
public:
    bool update(const j1_hold_input& input) noexcept;
    void reset() noexcept;

private:
    bool latched_{};
};

class runtime_policy {
public:
    runtime_policy(bool config_valid, bool j1_registered,
                   bsp::can::telemetry can_baseline) noexcept;

    runtime_policy_output update(const runtime_policy_input& input) noexcept;
    void latch(runtime_fault fault) noexcept;
    bool fault_latched() const noexcept;
    runtime_fault faults() const noexcept;
    arm_safety_state reported_state(arm_safety_state controller_state) const noexcept;

private:
    std::uint32_t fault_mask_{};
    bool config_valid_{};
};

class watchdog_phase {
public:
    bool advance() noexcept;

private:
    std::uint8_t phase_{};
};

bool remote_snapshot_fresh(bool seen, std::uint32_t sample_tick,
                           std::uint32_t now,
                           std::uint32_t freshness_ticks) noexcept;
bool should_enable_outputs(const runtime_policy_output& policy,
                           arm_safety_state controller_state) noexcept;
bool should_hold_j2_output(bool pwm_active,
                           const runtime_policy_output& policy) noexcept;
arm_safety_input controller_safety_for(
    const runtime_policy_output& policy) noexcept;
bool trusted_release_observed(
    const runtime_policy_output& policy) noexcept;
bool deadline_reached(std::uint32_t now,
                      std::uint32_t deadline) noexcept;

struct telemetry {
    arm_safety_state state{};
    runtime_fault faults{runtime_fault::none};
    bool watchdog_sampled{};
    std::uint32_t loop_count{};
    std::uint32_t overrun_count{};
    std::uint32_t remote_update_count{};
    control_mode mode{control_mode::neutral};
    bool j1_zero_captured{};
    float j1_axis{};
    float j1_target_position_rad{};
    float j1_measured_position_rad{};
    float j1_target_velocity_rad_s{};
    float j1_measured_velocity_rad_s{};
    std::int16_t j1_feedback_current_raw{};
    float j1_gravity_current_raw{};
    std::int16_t j1_current_raw{};
    float j1_stall_elapsed_s{};
    j1_stall_direction j1_stall_blocked_direction{
        j1_stall_direction::none};
    bool j1_stall_active{};
    bool j1_hold_active{};
    float j2_axis{};
    std::uint32_t j2_pulse_us{};
    bool j2_pwm_enabled{};
    float j3_axis{};
    std::uint32_t j3_pulse_us{};
    bool j3_pwm_enabled{};
    float gripper_axis{};
    std::uint32_t gripper_pulse_us{};
    bool gripper_pwm_enabled{};
    bool outputs_enabled{};
    bsp::can::telemetry can{};
};

namespace runtime
{

void start(const configuration& config) noexcept;
telemetry debug_state() noexcept;

} // namespace runtime

} // namespace vehicle::arm
