#pragma once

#include <cstdint>

namespace vehicle::arm
{

// 純控制層的機械臂解鎖狀態機。左撥桿選模式，右撥桿必須先釋放再上撥，
// 且四個手動軸回中後才授權；remote、J1、CAN 或設定失效時保持 fail-closed。
enum class arm_safety_state : std::uint8_t {
    disabled,
    waiting_remote,
    waiting_j1,
    armed,
    fault_latched,
};

struct arm_safety_input {
    bool remote_online{};
    bool arm_mode_selected{};
    bool enable_switches_up{};
    bool manual_axes_centered{};
    bool j1_online{};
    bool can_healthy{};
    bool config_valid{};
    bool mode_independent_unlock{};
    bool right_switch_up{};
};

class arm_safety_gate {
public:
    arm_safety_state update(const arm_safety_input& input) noexcept;
    bool output_enabled() const noexcept;
    void reset() noexcept;

private:
    arm_safety_state state_{arm_safety_state::disabled};
    bool arm_position_released_{};
    bool previous_enable_switches_up_{};
    bool fault_release_observed_{};
};

} // namespace vehicle::arm
