#include "vehicle/chassis/control/kinematics.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::chassis
{
namespace
{

// 運動學只接受正常有限數字；NaN/Inf 不能流入比例縮放或 PI。
bool finite(float value) noexcept
{
    return std::isfinite(value);
}

bool valid(const body_velocity& command, const geometry& g) noexcept
{
    // 半輪距允許為 0 是為了純演算法邊界測試；真實四輪車仍應填入實測正值。
    return finite(command.vx_mps) && finite(command.vy_mps) &&
           finite(command.yaw_rad_s) && finite(g.wheel_radius_m) &&
           finite(g.half_wheelbase_m) && finite(g.half_track_m) &&
           finite(g.max_wheel_rad_s) && g.wheel_radius_m > 0.0F &&
           g.half_wheelbase_m >= 0.0F && g.half_track_m >= 0.0F &&
           g.max_wheel_rad_s > 0.0F;
}

} // namespace

std::optional<wheel_vector> inverse_kinematics(
    const body_velocity& command, const geometry& g) noexcept
{
    if (!valid(command, g))
    {
        return std::nullopt;
    }

    // k 是車體旋轉時，輪速對 yaw 的等效力臂。
    const float k = g.half_wheelbase_m + g.half_track_m;

    // X 型麥輪逆解，輪序為 FL/FR/RL/RR：
    //   前進 +vx： + + + +
    //   左移 +vy： - + + -
    //   逆時針：   - + - +
    // 這裡產生的是「車體輪正方向」，尚未套用各馬達的 encoder 正負方向。
    wheel_vector out{{
        (command.vx_mps - command.vy_mps - k * command.yaw_rad_s) /
            g.wheel_radius_m,
        (command.vx_mps + command.vy_mps + k * command.yaw_rad_s) /
            g.wheel_radius_m,
        (command.vx_mps + command.vy_mps - k * command.yaw_rad_s) /
            g.wheel_radius_m,
        (command.vx_mps - command.vy_mps + k * command.yaw_rad_s) /
            g.wheel_radius_m,
    }};

    // 找出四輪中需求最大的絕對角速度，用於統一限幅。
    float maximum_magnitude = 0.0F;
    for (const float wheel_rad_s : out.rad_s)
    {
        if (!finite(wheel_rad_s))
        {
            return std::nullopt;
        }
        maximum_magnitude = std::max(
            maximum_magnitude, std::fabs(wheel_rad_s));
    }

    if (maximum_magnitude > g.max_wheel_rad_s)
    {
        // 四輪一起乘同一比例，保留 vx/vy/yaw 的方向組合；若逐輪 clamp，
        // 底盤的實際運動方向會被改變。
        const float scale = g.max_wheel_rad_s / maximum_magnitude;
        for (float& wheel_rad_s : out.rad_s)
        {
            wheel_rad_s *= scale;
        }
    }

    return out;
}

} // namespace vehicle::chassis
