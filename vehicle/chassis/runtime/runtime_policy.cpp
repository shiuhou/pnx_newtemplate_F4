#include "vehicle/chassis/runtime/runtime.hpp"

namespace vehicle::chassis
{

runtime_policy::runtime_policy(bool config_valid, bool all_registered,
                               bsp::can::telemetry can_baseline) noexcept
    : can_baseline_(can_baseline), config_valid_(config_valid)
{
    // 啟動條件只評估一次並立即 latch；即使後面條件暫時變好，也不把
    // 「曾在不完整狀態啟動」誤當成可以自動恢復的正常啟動。
    if (!config_valid_)
    {
        latch(runtime_fault::invalid_config);
    }
    if (!all_registered)
    {
        latch(runtime_fault::registration_failed);
    }
    if (can_baseline_.bus_state != bsp::can::state::active)
    {
        latch(runtime_fault::can_changed);
    }
}

runtime_policy_output runtime_policy::update(
    const runtime_policy_input& input) noexcept
{
    // remoter service 可能支援多種來源；MyCar 手動模式只信任 fresh DR16。
    const bool remote_online =
        !input.remote.offline &&
        input.remote.active_source == remoter::source::dr16;
    // 目前首版手動閉環以左右撥桿同時 UP 作為解鎖請求。
    const bool arm_switches_up =
        input.remote.left_sw == remoter::sw_state::up &&
        input.remote.right_sw == remoter::sw_state::up;
    // CAN 保持 active 還不夠；啟動後新增的 error/drop/fault 也視為安全事件。
    const bool retained_can_unchanged =
        input.can.error_count == can_baseline_.error_count &&
        input.can.drop_count == can_baseline_.drop_count &&
        input.can.fault_epoch == can_baseline_.fault_epoch;
    const bool can_healthy =
        input.can.bus_state == bsp::can::state::active &&
        retained_can_unchanged;

    if (!retained_can_unchanged)
    {
        // 本設計不在行駛中自動恢復 CAN；人工查明原因後重新上電／驗證。
        latch(runtime_fault::can_changed);
    }
    if (input.overrun)
    {
        latch(runtime_fault::overrun);
    }

    // 從同一份判定結果同時產生 manual 與 safety，降低兩條資料路徑語意分歧。
    runtime_policy_output output{};
    output.manual = {
        remote_online,
        arm_switches_up,
        input.remote.left_x,
        input.remote.left_y,
        input.remote.right_x,
    };
    output.safety = {
        remote_online,
        arm_switches_up,
        input.watchdog_sampled && input.handler_all_online,
        can_healthy,
        config_valid_,
    };
    output.fault_latched = fault_latched();
    // force_zero 是 runtime 的最後保險：即使 controller 因 bug 報告 armed，
    // 任一硬體健康條件失敗仍不會選中它的 current。
    output.force_zero = output.fault_latched || !output.safety.can_healthy ||
                        !output.safety.all_motors_online ||
                        !output.safety.remote_online ||
                        !output.safety.config_valid;
    return output;
}

void runtime_policy::latch(runtime_fault fault) noexcept
{
    // 位元 OR 會保留所有根因，不讓較新的故障覆蓋較早的故障。
    fault_mask_ |= static_cast<std::uint32_t>(fault);
}

bool runtime_policy::fault_latched() const noexcept
{
    return fault_mask_ != 0U;
}

runtime_fault runtime_policy::faults() const noexcept
{
    return static_cast<runtime_fault>(fault_mask_);
}

safety_state runtime_policy::reported_state(
    safety_state controller_state) const noexcept
{
    // 對外只要有 runtime fault 就報 fault_latched，但根因仍保存在 faults()。
    return fault_latched() ? safety_state::fault_latched : controller_state;
}

bool watchdog_phase::advance() noexcept
{
    // 每四個 5 ms 控制週期檢查一次馬達存活，即 20 ms watchdog 視窗。
    ++phase_;
    if (phase_ == 4U)
    {
        phase_ = 0U;
        return true;
    }
    return false;
}

bool remote_snapshot_fresh(bool seen, std::uint32_t sample_tick,
                           std::uint32_t now,
                           std::uint32_t freshness_ticks) noexcept
{
    // unsigned 相減保留 tick wrap-around 下的正確新鮮度比較。
    return seen && (now - sample_tick) <= freshness_ticks;
}

bool should_set_current(const runtime_policy_output& policy,
                        safety_state controller_state) noexcept
{
    // 必須同時滿足兩層：runtime 沒有 force_zero，controller safety 已 armed。
    return !policy.force_zero && controller_state == safety_state::armed;
}

safety_input controller_safety_for(
    const runtime_policy_output& policy) noexcept
{
    // controller 不認識 runtime_fault，因此把 fault_latched 折進 config_valid。
    safety_input result = policy.safety;
    result.config_valid = result.config_valid && !policy.fault_latched;
    return result;
}

bool trusted_release_observed(
    const runtime_policy_output& policy) noexcept
{
    // 離線時殘留的非 UP 值不能用來清除 PI 或建立重新解鎖歷史。
    return policy.manual.online && !policy.manual.arm_switches_up;
}

bool deadline_reached(std::uint32_t now, std::uint32_t deadline) noexcept
{
    // 轉回 signed 差值，讓 32-bit ThreadX tick wrap-around 仍可判定 deadline。
    return static_cast<std::int32_t>(now - deadline) >= 0;
}

} // namespace vehicle::chassis
