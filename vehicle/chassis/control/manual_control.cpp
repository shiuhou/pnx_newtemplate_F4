#include "vehicle/chassis/control/manual_control.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::chassis
{
namespace
{

// 將 std::isfinite 包成局部函式，讓後面的輸入檢查保持可讀。
bool finite(float value) noexcept
{
    return std::isfinite(value);
}

// 設定必須可計算且真的允許某個非零速度；方向符號只接受 +/-1，
// 防止錯把比例、角度或尚未填寫的 0 當成方向。
bool valid_limits(const manual_limits& limits) noexcept
{
    return finite(limits.deadband) && limits.deadband >= 0.0F &&
           limits.deadband < 1.0F && finite(limits.max_vx_mps) &&
           limits.max_vx_mps > 0.0F && finite(limits.max_vy_mps) &&
           limits.max_vy_mps > 0.0F && finite(limits.max_yaw_rad_s) &&
           limits.max_yaw_rad_s > 0.0F && finite(limits.vx_sign) &&
           finite(limits.vy_sign) && finite(limits.yaw_sign) &&
           std::fabs(limits.vx_sign) == 1.0F &&
           std::fabs(limits.vy_sign) == 1.0F &&
           std::fabs(limits.yaw_sign) == 1.0F;
}

float apply_deadband(float axis, float deadband) noexcept
{
    // 例：deadband=0.05 時，|axis|<=0.05 都輸出 0；0.05..1 則線性
    // 重映射為 0..1，避免跨出死區後突然從 0 跳到 0.05。
    const float bounded = std::clamp(axis, -1.0F, 1.0F);
    const float magnitude = std::fabs(bounded);
    if (magnitude <= deadband)
    {
        return 0.0F;
    }

    const float scaled = (magnitude - deadband) / (1.0F - deadband);
    return std::copysign(scaled, bounded);
}

} // namespace

body_velocity map_manual(const manual_input& input,
                         const manual_limits& limits) noexcept
{
    // 這一層再次檢查 online/up，不能只信任 runtime 先前做過的判斷。
    // 任何非有限搖桿值也視為不可信，直接產生零速度。
    if (!input.online || !input.arm_switches_up || !finite(input.left_x) ||
        !finite(input.left_y) || !finite(input.right_x) ||
        !valid_limits(limits))
    {
        return {};
    }

    // 車體座標：+vx 向前、+vy 向左、+yaw 俯視逆時針。
    // config 內的 sign 讓操作者的搖桿直覺可以獨立於 DR16 原始軸正負。
    return {
        apply_deadband(input.left_y, limits.deadband) *
            limits.max_vx_mps * limits.vx_sign,
        apply_deadband(input.left_x, limits.deadband) *
            limits.max_vy_mps * limits.vy_sign,
        apply_deadband(input.right_x, limits.deadband) *
            limits.max_yaw_rad_s * limits.yaw_sign,
    };
}

} // namespace vehicle::chassis
