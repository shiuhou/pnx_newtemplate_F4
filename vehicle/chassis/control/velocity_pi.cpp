#include "vehicle/chassis/control/velocity_pi.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vehicle::chassis
{
namespace
{

// RoboMaster C610 v1.0：raw torque-current 指令 +/-10000 對應 +/-10 A。
// 這是 C610 的轉矩電流命令，不等於電池端或電調輸入端實測電流。
constexpr float c610_max_current_raw = 10000.0F;

} // namespace

bool valid(const velocity_pi_config& config) noexcept
{
    // 至少要有可工作的 P，或同時具備非零 I 與積分上限；current limit=0
    // 被視為尚未授權閉環出力，因此整組設定無效並保持 fail-closed。
    return std::isfinite(config.kp) && config.kp >= 0.0F &&
           std::isfinite(config.ki_per_s) && config.ki_per_s >= 0.0F &&
           std::isfinite(config.integral_limit_raw) &&
           config.integral_limit_raw >= 0.0F &&
           (config.kp > 0.0F ||
            (config.ki_per_s > 0.0F &&
             config.integral_limit_raw > 0.0F)) &&
           std::isfinite(config.current_limit_raw) &&
           config.current_limit_raw > 0.0F &&
           config.current_limit_raw <= c610_max_current_raw;
}

velocity_pi::velocity_pi(velocity_pi_config config) noexcept
    : config_(config), config_valid_(valid(config))
{
}

std::int16_t velocity_pi::update(float target_rad_s,
                                 float measured_rad_s,
                                 float dt_s) noexcept
{
    // 無效設定、NaN/Inf 或錯誤 dt 都不能嘗試沿用上一筆輸出。
    if (!config_valid_ || !std::isfinite(target_rad_s) ||
        !std::isfinite(measured_rad_s) || !std::isfinite(dt_s) ||
        dt_s <= 0.0F)
    {
        reset();
        return 0;
    }

    // 正誤差表示馬達比目標慢，需要增加正向 torque-current。
    const float error = target_rad_s - measured_rad_s;
    if (!std::isfinite(error))
    {
        reset();
        return 0;
    }

    // Ki 的定義以「每秒」為基準，因此積分時必須乘實際 dt_s。
    const float candidate_i = std::clamp(
        integral_raw_ + config_.ki_per_s * error * dt_s,
        -config_.integral_limit_raw,
        config_.integral_limit_raw);
    // 先算理想 PI，再用 current_limit_raw 做最後輸出限幅。
    const float unsaturated = config_.kp * error + candidate_i;
    const float saturated = std::clamp(
        unsaturated, -config_.current_limit_raw, config_.current_limit_raw);
    if (unsaturated == saturated || error * unsaturated < 0.0F)
    {
        // 未飽和時正常接受積分；若已飽和，只接受能把輸出拉回範圍的積分方向。
        integral_raw_ = candidate_i;
    }

    if (!std::isfinite(saturated) ||
        saturated < static_cast<float>(std::numeric_limits<std::int16_t>::min()) ||
        saturated > static_cast<float>(std::numeric_limits<std::int16_t>::max()))
    {
        reset();
        return 0;
    }

    return static_cast<std::int16_t>(std::lround(saturated));
}

void velocity_pi::reset() noexcept
{
    integral_raw_ = 0.0F;
}

} // namespace vehicle::chassis
