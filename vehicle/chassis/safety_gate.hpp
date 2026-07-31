#pragma once

#include <cstdint>

namespace vehicle::chassis
{

enum class safety_state : std::uint8_t {
    disabled,
    waiting_remote,
    waiting_motors,
    armed,
    fault_latched,
};

struct safety_input {
    bool remote_online{};
    bool arm_switches_up{};
    bool all_motors_online{};
    bool can_healthy{};
    bool config_valid{};
};

class safety_gate {
public:
    safety_state update(const safety_input& input) noexcept;
    bool output_enabled() const noexcept;
    void reset() noexcept;

private:
    safety_state state_{safety_state::disabled};
    bool arm_position_released_{};
    bool previous_arm_switches_up_{};
    bool fault_release_observed_{};
};

} // namespace vehicle::chassis
