#include "vehicle/chassis/control/safety_gate.hpp"

namespace vehicle::chassis
{

safety_state safety_gate::update(const safety_input& input) noexcept
{
    // 離線時保留的撥桿值不可信，不能建立「已釋放」或「剛切到 UP」歷史。
    const bool switch_sample_valid = input.remote_online;
    const bool arm_rising = switch_sample_valid &&
                            input.arm_switches_up &&
                            !previous_arm_switches_up_;

    // 任何 fresh 的非 UP 都表示操作者已做出明確釋放動作。
    if (switch_sample_valid && !input.arm_switches_up)
    {
        arm_position_released_ = true;
        if (state_ == safety_state::fault_latched)
        {
            fault_release_observed_ = true;
        }
    }

    if (state_ == safety_state::fault_latched)
    {
        // fault_latched 不在 update() 中自行恢復；這裡只記錄新的撥桿歷史。
        if (switch_sample_valid)
        {
            previous_arm_switches_up_ = input.arm_switches_up;
        }
        return state_;
    }

    if (state_ == safety_state::armed)
    {
        // 健康條件失效與操作者主動釋放的語意不同：前者鎖故障，後者正常停車。
        if (!input.config_valid || !input.remote_online ||
            !input.all_motors_online || !input.can_healthy)
        {
            state_ = safety_state::fault_latched;
            fault_release_observed_ =
                switch_sample_valid && !input.arm_switches_up;
        }
        else if (!input.arm_switches_up)
        {
            state_ = safety_state::disabled;
        }

        if (switch_sample_valid)
        {
            previous_arm_switches_up_ = input.arm_switches_up;
        }
        return state_;
    }

    // 尚未 armed 時依序報告最直接的等待原因；只有全部健康且出現合法
    // UP 上升沿，才能由 disabled/waiting 狀態轉入 armed。
    if (!input.config_valid)
    {
        state_ = safety_state::disabled;
    }
    else if (!input.remote_online)
    {
        state_ = safety_state::waiting_remote;
    }
    else if (!input.all_motors_online || !input.can_healthy)
    {
        state_ = safety_state::waiting_motors;
    }
    else if (arm_position_released_ && arm_rising)
    {
        // 防止遙控器或控制板在 UP/UP 狀態上電後直接讓車輛動作。
        state_ = safety_state::armed;
    }
    else
    {
        state_ = safety_state::disabled;
    }

    if (switch_sample_valid)
    {
        previous_arm_switches_up_ = input.arm_switches_up;
    }
    return state_;
}

bool safety_gate::output_enabled() const noexcept
{
    // 所有輸出權限最後都收斂到這一個明確狀態。
    return state_ == safety_state::armed;
}

void safety_gate::reset() noexcept
{
    // reset 不是無條件清錯；必須先在 fault_latched 中看到 fresh 的人工釋放。
    if (state_ == safety_state::fault_latched && fault_release_observed_)
    {
        state_ = safety_state::disabled;
        fault_release_observed_ = false;
    }
}

} // namespace vehicle::chassis
