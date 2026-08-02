#pragma once

#include "vehicle/chassis/runtime/config.hpp"

#include <bsp_can.hpp>
#include <types.hpp>

#include <array>
#include <cstdint>

namespace vehicle::chassis
{

// runtime_policy 會把這些系統級故障保存成位元遮罩。
// 一旦任一位元被 latch，本次上電週期不再允許自動恢復非零輸出。
enum class runtime_fault : std::uint32_t {
    none = 0U,                       // 尚未發現 runtime 故障。
    invalid_config = 1U << 0U,      // config.cpp 的必要參數或 5 ms 契約非法。
    registration_failed = 1U << 1U, // 任一 M2006 無法註冊至 DJI handler/CAN。
    remoter_init_failed = 1U << 2U, // DR16/remoter service 初始化失敗。
    subscribe_failed = 1U << 3U,    // 無法訂閱 remoter::state 訊息。
    thread_create_failed = 1U << 4U,// control 或 remote ingest thread 建立失敗。
    can_changed = 1U << 5U,         // CAN error/drop/fault epoch 相對基線發生變化。
    overrun = 1U << 6U,             // 5 ms 控制週期在 deadline 前未完成。
};

// control thread 每一週期收集的原始系統觀測值。
struct runtime_policy_input {
    remoter::state remote{};   // 經新鮮度處理後的 DR16 快照。
    bsp::can::telemetry can{}; // 本週期 CAN1 狀態與計數器。
    bool watchdog_sampled{};   // 至少執行過一次馬達 alive_check。
    bool handler_all_online{}; // 最近一次 alive_check 是否四顆都有新回饋。
    bool overrun{};            // 外部測試或 runtime 偵測到本週期超時。
};

// runtime_policy 把底層觀測轉成 controller 能理解的 manual/safety 資料，
// 同時給出 runtime 自己的最終零輸出裁決。
struct runtime_policy_output {
    manual_input manual{}; // 已確認 source/online/up 的搖桿資料。
    safety_input safety{}; // controller safety gate 的本週期健康條件。
    bool force_zero{true}; // true 時 runtime 無條件 relax 四顆馬達。
    bool fault_latched{};  // true 時 reported_state 會顯示 fault_latched。
};

// 將 ThreadX/CAN/remoter 的硬體狀態與純 controller 隔離的安全策略層。
class runtime_policy {
public:
    // can_baseline 是啟動時快照；後續計數器只要改變就視為新的 CAN 事件。
    runtime_policy(bool config_valid, bool all_registered,
                   bsp::can::telemetry can_baseline) noexcept;

    // 輸入一次 runtime 觀測並產生 manual/safety/force_zero 決策。
    runtime_policy_output update(const runtime_policy_input& input) noexcept;
    // 將一個或多個 runtime_fault OR 進故障位元遮罩。
    void latch(runtime_fault fault) noexcept;
    // 是否存在任何已鎖定故障。
    bool fault_latched() const noexcept;
    // 回傳完整故障位元遮罩，供 telemetry 判讀根因。
    runtime_fault faults() const noexcept;
    // runtime fault 的優先級高於 controller state。
    safety_state reported_state(safety_state controller_state) const noexcept;

private:
    bsp::can::telemetry can_baseline_{};
    std::uint32_t fault_mask_{};
    bool config_valid_{};
};

class watchdog_phase {
public:
    // 每呼叫四次回傳一次 true；5 ms loop 下即每 20 ms 執行 alive_check。
    bool advance() noexcept;

private:
    std::uint8_t phase_{};
};

// 處理 32-bit tick wrap-around 的 DR16 快照新鮮度判定。
bool remote_snapshot_fresh(bool seen, std::uint32_t sample_tick,
                           std::uint32_t now,
                           std::uint32_t freshness_ticks) noexcept;
// 同時通過 runtime force_zero 與 controller armed 才能選擇非零 current。
bool should_set_current(const runtime_policy_output& policy,
                        safety_state controller_state) noexcept;
// 把 runtime fault 也折入 controller 的 config_valid，避免 controller 自行 armed。
safety_input controller_safety_for(
    const runtime_policy_output& policy) noexcept;
// 只有 fresh DR16 明確離開 UP/UP，才算可信的人工釋放。
bool trusted_release_observed(
    const runtime_policy_output& policy) noexcept;
// 以 signed 差值比較 ThreadX deadline，正確處理 tick wrap-around。
bool deadline_reached(std::uint32_t now,
                      std::uint32_t deadline) noexcept;

// debug_state() 回傳的唯讀診斷快照。它描述最近完成的一個控制週期，
// 不能反過來當成控制輸入，也不能把「已呼叫 CAN send」當成實體送達證據。
struct telemetry {
    safety_state state{};                         // 綜合 controller/runtime 後的狀態。
    runtime_fault faults{runtime_fault::none};    // 已 latch 的系統級故障位元。
    bool watchdog_sampled{};                      // 馬達 online 結論是否已有真實樣本。
    std::uint32_t loop_count{};                   // 已進入的 5 ms 控制週期數。
    std::uint32_t overrun_count{};                // 被偵測為超時的週期累計。
    std::uint32_t remote_update_count{};           // 最近 DR16 快照的更新序號。
    // Body-frame FL/FR/RL/RR wheel targets before motor direction mapping.
    std::array<float, 4U> target_rad_s{};
    // Raw signed M2006 gearbox-output feedback.
    std::array<float, 4U> measured_rad_s{};
    std::array<std::int16_t, 4U> current_raw{};   // 本週期實際選中的 FL/FR/RL/RR 指令。
    bsp::can::telemetry can{};                    // 同週期開始時取得的 CAN1 快照。
};

namespace runtime
{

// 註冊四顆 M2006、建立安全策略與控制 thread；每次上電只允許成功嘗試一次。
void start(const configuration& config) noexcept;
// 以短暫關中斷方式複製最近 telemetry，供 debugger 或未來報告通道讀取。
telemetry debug_state() noexcept;

} // namespace runtime

} // namespace vehicle::chassis
