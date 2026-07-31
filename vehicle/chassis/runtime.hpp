#pragma once

#include "vehicle/chassis/config.hpp"

#include <bsp_can.hpp>
#include <types.hpp>

#include <array>
#include <cstdint>

namespace vehicle::chassis
{

enum class runtime_fault : std::uint32_t {
    none = 0U,
    invalid_config = 1U << 0U,
    registration_failed = 1U << 1U,
    remoter_init_failed = 1U << 2U,
    subscribe_failed = 1U << 3U,
    thread_create_failed = 1U << 4U,
    can_changed = 1U << 5U,
    overrun = 1U << 6U,
};

struct runtime_policy_input {
    remoter::state remote{};
    bsp::can::telemetry can{};
    bool watchdog_sampled{};
    bool handler_all_online{};
    bool overrun{};
};

struct runtime_policy_output {
    manual_input manual{};
    safety_input safety{};
    bool force_zero{true};
    bool fault_latched{};
};

class runtime_policy {
public:
    runtime_policy(bool config_valid, bool all_registered,
                   bsp::can::telemetry can_baseline) noexcept;

    runtime_policy_output update(const runtime_policy_input& input) noexcept;
    void latch(runtime_fault fault) noexcept;
    bool fault_latched() const noexcept;
    runtime_fault faults() const noexcept;
    safety_state reported_state(safety_state controller_state) const noexcept;

private:
    bsp::can::telemetry can_baseline_{};
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
bool should_set_current(const runtime_policy_output& policy,
                        safety_state controller_state) noexcept;
safety_input controller_safety_for(
    const runtime_policy_output& policy) noexcept;
bool trusted_release_observed(
    const runtime_policy_output& policy) noexcept;
bool deadline_reached(std::uint32_t now,
                      std::uint32_t deadline) noexcept;

struct telemetry {
    safety_state state{};
    runtime_fault faults{runtime_fault::none};
    bool watchdog_sampled{};
    std::uint32_t loop_count{};
    std::uint32_t overrun_count{};
    std::uint32_t remote_update_count{};
    // Body-frame FL/FR/RL/RR wheel targets before motor direction mapping.
    std::array<float, 4U> target_rad_s{};
    // Raw signed M2006 gearbox-output feedback.
    std::array<float, 4U> measured_rad_s{};
    std::array<std::int16_t, 4U> current_raw{};
    bsp::can::telemetry can{};
};

namespace runtime
{

void start(const configuration& config) noexcept;
telemetry debug_state() noexcept;

} // namespace runtime

} // namespace vehicle::chassis
