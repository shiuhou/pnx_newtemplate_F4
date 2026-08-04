#pragma once

#include "vehicle/chassis/control/controller.hpp"

#include <array>

namespace vehicle::chassis
{

// MyCar 的完整控制設定。成員順序與 config.cpp 的巢狀 initializer 相同，
// 閱讀數字時應先在這裡確認每一組值代表的型別與單位。
struct configuration {
    // 輪徑、輪距與輪端角速度上限。
    ::vehicle::chassis::geometry geometry;
    // DR16 死區、三軸車體速度上限與搖桿方向。
    manual_limits manual;
    // 正常行駛的平移／yaw 加減速率；安全停車仍立即歸零。
    command_slew_config command_slew;
    // 車體輪正方向到馬達編碼器正方向的映射，順序 FL/FR/RL/RR。
    std::array<float, 4U> motor_direction;
    // 四顆 M2006 共用的速度 PI 與 C610 raw-current 上限。
    velocity_pi_config pi;
    // 每次 controller::update() 之間的秒數；目前必須是 0.005 s。
    float control_period_s;
};

// 回傳編譯進韌體的 MyCar 設定。
configuration mycar_configuration() noexcept;
// 檢查完整設定是否可允許控制；不得用猜測值取代 sentinel 零值來強制出力。
bool valid(const configuration& config) noexcept;

} // namespace vehicle::chassis
