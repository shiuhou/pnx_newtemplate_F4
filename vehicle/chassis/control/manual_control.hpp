#pragma once

#include "vehicle/chassis/common/types.hpp"

namespace vehicle::chassis
{

// runtime 從 remoter::state 擷取後，交給純控制層的最小 DR16 輸入。
// 搖桿值已正規化到 -1..+1；此結構不接觸 UART 或 DBUS 原始封包。
struct manual_input {
    bool online{};          // true：資料來源確為 DR16，且未超過新鮮度期限。
    bool arm_switches_up{}; // true：本車的右解鎖撥桿位於 UP；仍須通過 safety gate。
    float left_x{};         // 左搖桿水平軸，-1..+1，用於 vy。
    float left_y{};         // 左搖桿垂直軸，-1..+1，用於 vx。
    float right_x{};        // 右搖桿水平軸，-1..+1，用於 yaw。
};

// 把正規化搖桿量轉換為實際車體速度時使用的限制與方向。
struct manual_limits {
    float deadband{};       // 中心死區，範圍 0..1；死區外會重新映射至完整行程。
    float max_vx_mps{};     // 左搖桿 Y 滿量時的前後速度上限，m/s。
    float max_vy_mps{};     // 左搖桿 X 滿量時的橫移速度上限，m/s。
    float max_yaw_rad_s{};  // 右搖桿 X 滿量時的旋轉速度上限，rad/s。
    float vx_sign{};        // 修正 DR16 前後軸方向，只能為 +1 或 -1。
    float vy_sign{};        // 修正 DR16 橫移軸方向，只能為 +1 或 -1。
    float yaw_sign{};       // 修正 DR16 旋轉軸方向，只能為 +1 或 -1。
};

// 只做「搖桿 -> 車體速度」映射；離線、未解鎖或設定非法時回傳全零。
body_velocity map_manual(const manual_input& input,
                         const manual_limits& limits) noexcept;

} // namespace vehicle::chassis
