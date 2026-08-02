#pragma once

#include "vehicle/chassis/common/types.hpp"

#include <optional>

namespace vehicle::chassis
{

// X 型麥輪逆運動學：將底盤中心的 vx/vy/yaw 命令轉成四輪輸出端角速度。
// 回傳順序固定為 FL/FR/RL/RR；輸入或幾何非法時回傳 nullopt，讓上層歸零。
std::optional<wheel_vector> inverse_kinematics(
    const body_velocity& command, const geometry& g) noexcept;

} // namespace vehicle::chassis
