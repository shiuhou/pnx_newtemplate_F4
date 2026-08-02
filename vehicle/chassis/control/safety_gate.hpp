#pragma once

#include <cstdint>

namespace vehicle::chassis
{

// 純控制層的解鎖狀態機。runtime 可能另外 latch 系統故障；
// 只有 state==armed 時 controller 才允許計算非零 current。
enum class safety_state : std::uint8_t {
    disabled,       // 條件尚未完成，或操作者主動把撥桿移離 UP。
    waiting_remote, // 尚未收到新鮮且來源正確的 DR16 資料。
    waiting_motors, // DR16 正常，但四馬達 online/CAN healthy 尚未同時成立。
    armed,          // 已觀察「先釋放、後 UP」上升沿，允許 controller 出力。
    fault_latched,  // armed 後健康條件失效；必須人工釋放後才可 reset。
};

// 每一個控制週期提供給 safety gate 的可信條件。
struct safety_input {
    bool remote_online{};     // true：新鮮資料來自 DR16，不是其他 source 或舊快照。
    bool arm_switches_up{};   // true：左右撥桿當前都為 UP。
    bool all_motors_online{}; // true：四顆 M2006 在 watchdog 視窗內都有新回饋。
    bool can_healthy{};       // true：CAN active，且 error/drop/fault epoch 未變。
    bool config_valid{};      // true：幾何、DR16、方向、PI 與週期全部合法。
};

// 這個類別只判斷解鎖與故障狀態，不直接發 CAN，也不計算馬達電流。
class safety_gate {
public:
    // 消費一次健康快照並推進狀態；解鎖需先觀察非 UP，再觀察 fresh UP 上升沿。
    safety_state update(const safety_input& input) noexcept;
    // controller 用這個結果決定 PI 是否有權產生非零輸出。
    bool output_enabled() const noexcept;
    // 僅在 fault_latched 且已觀察到人工釋放時，回到 disabled。
    void reset() noexcept;

private:
    safety_state state_{safety_state::disabled}; // 目前狀態。
    bool arm_position_released_{};               // 自上電後至少見過一次非 UP。
    bool previous_arm_switches_up_{};            // 用來辨認本週期是否為 UP 上升沿。
    bool fault_release_observed_{};               // 故障鎖定後是否已見人工釋放。
};

} // namespace vehicle::chassis
