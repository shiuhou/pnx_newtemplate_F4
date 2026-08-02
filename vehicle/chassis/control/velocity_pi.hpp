#pragma once

#include <cstdint>

namespace vehicle::chassis
{

// 單顆 M2006 的速度 PI 設定。速度以減速箱輸出端 rad/s 表示；
// PI 輸出不是 PWM，而是送給 C610 的 signed raw torque-current 指令。
struct velocity_pi_config {
    float kp{};                 // 比例增益，單位 raw/(rad/s)。
    float ki_per_s{};           // 積分增益，單位 raw/(rad/s*s)。
    float integral_limit_raw{}; // 積分項自身的絕對值上限，防止累積過大。
    float current_limit_raw{};  // 最終輸出的絕對值上限；C610 硬上限為 10000。
};

// 檢查增益、積分限制與 current limit 是否有限且落在安全契約內。
bool valid(const velocity_pi_config& config) noexcept;

// 一個物件只控制一顆馬達；controller 內會建立四個互不共享積分的實例。
class velocity_pi {
public:
    explicit velocity_pi(velocity_pi_config config) noexcept;

    // 以本週期的目標、量測與 dt 更新 PI，回傳要放進 CAN 0x200 的 raw current。
    std::int16_t update(float target_rad_s,
                        float measured_rad_s,
                        float dt_s) noexcept;
    // 停止、失效或重新解鎖時清除積分，避免帶入上一段運動的歷史誤差。
    void reset() noexcept;

private:
    velocity_pi_config config_{};
    float integral_raw_{}; // 此馬達跨控制週期保存的積分輸出。
    bool config_valid_{};  // 建構時鎖定；非法設定不會在 update 中嘗試自動修復。
};

} // namespace vehicle::chassis
