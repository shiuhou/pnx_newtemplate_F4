#pragma once

#include "pnx_modules/remoter/include/types.hpp"
#include "vehicle/arm/runtime/arm_runtime.hpp"
#include "vehicle/chassis/runtime/runtime.hpp"
#include "vehicle/combined/control/mode_router.hpp"

#include <bsp_can.hpp>

#include <cstdint>

namespace vehicle::combined
{

enum class runtime_fault : std::uint32_t {
    none = 0U,
    invalid_config = 1U << 0U,
    registration_failed = 1U << 1U,
    remoter_init_failed = 1U << 2U,
    subscribe_failed = 1U << 3U,
    thread_create_failed = 1U << 4U,
    pwm_failed = 1U << 5U,
    overrun = 1U << 6U,
    subsystem_fault = 1U << 7U,
};

struct telemetry {
    control_mode mode{control_mode::neutral};
    runtime_fault faults{runtime_fault::none};
    bool watchdog_sampled{};
    bool all_motors_online{};
    std::uint32_t loop_count{};
    std::uint32_t overrun_count{};
    std::uint32_t remote_update_count{};
    chassis::telemetry chassis{};
    arm::telemetry arm{};
    bsp::can::telemetry can{};
};

namespace runtime
{

void start(const chassis::configuration& chassis_config,
           const arm::configuration& arm_config) noexcept;
telemetry debug_state() noexcept;

} // namespace runtime

} // namespace vehicle::combined
