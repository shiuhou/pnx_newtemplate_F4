#pragma once

#include "vehicle/arm/control/position_pid.hpp"

namespace vehicle::arm
{

// 機械臂 runtime 目前先收斂到 J1 位置外環與固定 5 ms 控制週期。
struct configuration {
    position_pid_config j1_position;
    float j1_manual_position_rate_rad_per_s{}; // right_y 滿量時的目標位置斜率。
    float control_period_s{};                  // 目前契約要求 0.005 s。
};

bool valid(const configuration& config) noexcept;

} // namespace vehicle::arm
