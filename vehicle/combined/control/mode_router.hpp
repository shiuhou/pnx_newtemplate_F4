#pragma once

#include "pnx_modules/remoter/include/types.hpp"

#include <cstdint>

namespace vehicle::combined
{

enum class control_mode : std::uint8_t {
    neutral,
    chassis,
    arm,
};

struct mode_router_output {
    control_mode mode{control_mode::neutral};
    bool remote_online{};
    bool chassis_ready{};
    bool arm_axes_centered{};
    remoter::state chassis_remote{};
};

class mode_router {
public:
    mode_router_output update(const remoter::state& remote,
                              float chassis_deadband,
                              float arm_deadband) noexcept;
    void reset() noexcept;

private:
    bool chassis_ready_{};
};

} // namespace vehicle::combined
