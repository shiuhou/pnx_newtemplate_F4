#include "vehicle/chassis/control/controller.hpp"

#include <cmath>

namespace vehicle::chassis
{
namespace
{

// 以下三個函式只驗證純資料，讓 controller 本體可以專注於資料流與安全順序。
bool valid_geometry(const geometry& value) noexcept
{
    return std::isfinite(value.wheel_radius_m) &&
           value.wheel_radius_m > 0.0F &&
           std::isfinite(value.half_wheelbase_m) &&
           value.half_wheelbase_m >= 0.0F &&
           std::isfinite(value.half_track_m) &&
           value.half_track_m >= 0.0F &&
           std::isfinite(value.max_wheel_rad_s) &&
           value.max_wheel_rad_s > 0.0F;
}

bool valid_manual_limits(const manual_limits& value) noexcept
{
    return std::isfinite(value.deadband) && value.deadband >= 0.0F &&
           value.deadband < 1.0F && std::isfinite(value.max_vx_mps) &&
           value.max_vx_mps > 0.0F && std::isfinite(value.max_vy_mps) &&
           value.max_vy_mps > 0.0F &&
           std::isfinite(value.max_yaw_rad_s) &&
           value.max_yaw_rad_s > 0.0F && std::isfinite(value.vx_sign) &&
           std::fabs(value.vx_sign) == 1.0F &&
           std::isfinite(value.vy_sign) &&
           std::fabs(value.vy_sign) == 1.0F &&
           std::isfinite(value.yaw_sign) &&
           std::fabs(value.yaw_sign) == 1.0F;
}

bool valid_manual_sample(const manual_input& value) noexcept
{
    return std::isfinite(value.left_x) &&
           std::isfinite(value.left_y) &&
           std::isfinite(value.right_x);
}

bool valid_body_velocity(const body_velocity& value) noexcept
{
    return std::isfinite(value.vx_mps) &&
           std::isfinite(value.vy_mps) &&
           std::isfinite(value.yaw_rad_s);
}

} // namespace

bool valid(const controller_configuration& config) noexcept
{
    // 幾何、搖桿映射、PI 或任一馬達方向非法，都讓整個 controller 無效。
    if (!valid_geometry(config.geometry) ||
        !valid_manual_limits(config.manual) ||
        !valid(config.command_slew) || !valid(config.pi))
    {
        return false;
    }

    for (const float direction : config.motor_direction)
    {
        if (!std::isfinite(direction) || std::fabs(direction) != 1.0F)
        {
            return false;
        }
    }
    return true;
}

controller::controller(controller_configuration config) noexcept
    : config_(config),
      config_valid_(valid(config)),
      command_slew_(config.command_slew),
      pi_{velocity_pi{config.pi}, velocity_pi{config.pi},
          velocity_pi{config.pi}, velocity_pi{config.pi}}
{
    // 即使建構時設定非法仍保留物件；update 會因 config_valid_ 而 fail-closed。
}

controller_output controller::update(
    const manual_input& manual,
    const wheel_vector& measured_motor_rad_s,
    const safety_input& safety,
    float dt_s) noexcept
{
    // runtime_policy 與 manual snapshot 可能來自不同的複製時刻，因此 safety
    // 同時要求兩邊都認為 DR16 online/up，避免時間差造成誤解鎖。
    safety_input gated_safety = safety;
    gated_safety.remote_online = safety.remote_online && manual.online;
    gated_safety.arm_switches_up =
        safety.arm_switches_up && manual.arm_switches_up;
    const bool manual_valid = valid_manual_sample(manual);
    gated_safety.config_valid = safety.config_valid && config_valid_ &&
                                manual_valid;
    return update_body_velocity(
        map_manual(manual, config_.manual), measured_motor_rad_s,
        gated_safety, dt_s, manual_valid);
}

controller_output controller::update(
    const body_velocity& command,
    const wheel_vector& measured_motor_rad_s,
    const safety_input& safety,
    float dt_s) noexcept
{
    const bool command_valid = valid_body_velocity(command);
    safety_input gated_safety = safety;
    gated_safety.config_valid = safety.config_valid && config_valid_ &&
                                command_valid;
    return update_body_velocity(command, measured_motor_rad_s,
                                gated_safety, dt_s, command_valid);
}

controller_output controller::update_body_velocity(
    const body_velocity& command,
    const wheel_vector& measured_motor_rad_s,
    const safety_input& safety,
    float dt_s,
    bool command_valid) noexcept
{
    // 每个周期从全零输出开始；手动与自动从这里共用唯一控制链。
    controller_output output{};
    output.state = safety_.update(safety);

    // 安全事件與非法時間不能等待斜坡；當週期直接零輸出並清除全部控制狀態。
    if (!safety_.output_enabled() || !config_valid_ ||
        !command_valid || !std::isfinite(dt_s) || dt_s <= 0.0F)
    {
        command_slew_.reset();
        reset_pi();
        return output;
    }

    // 只有 armed 且資料可信時才推進斜坡；正常搖桿回中會在這裡平滑減速。
    const body_velocity limited_command = command_slew_.update(
        command, dt_s);
    const auto target = inverse_kinematics(
        limited_command, config_.geometry);
    if (!target.has_value())
    {
        command_slew_.reset();
        reset_pi();
        return output;
    }
    output.wheel_target_rad_s = *target;

    // 一顆回饋出現 NaN/Inf 就拒絕整組四輪輸出，不讓其他三輪繼續推車。
    for (const float measured : measured_motor_rad_s.rad_s)
    {
        if (!std::isfinite(measured))
        {
            command_slew_.reset();
            reset_pi();
            return output;
        }
    }

    std::array<float, 4U> motor_target_rad_s{};
    for (std::size_t index = 0U; index < pi_.size(); ++index)
    {
        // 逆解中的正輪速是車體定義；乘 motor_direction 後才成為這顆
        // 實體 M2006 encoder 應追蹤的正負目標。
        motor_target_rad_s[index] =
            output.wheel_target_rad_s.rad_s[index] *
            config_.motor_direction[index];
        const float error =
            motor_target_rad_s[index] - measured_motor_rad_s.rad_s[index];
        if (!std::isfinite(motor_target_rad_s[index]) ||
            !std::isfinite(error))
        {
            command_slew_.reset();
            reset_pi();
            return output;
        }
    }

    // 四顆 PI 使用相同 dt，但各自保存積分，輸出仍按 FL/FR/RL/RR 排列。
    for (std::size_t index = 0U; index < pi_.size(); ++index)
    {
        output.motor_current_raw[index] = pi_[index].update(
            motor_target_rad_s[index], measured_motor_rad_s.rad_s[index], dt_s);
    }
    return output;
}

void controller::reset() noexcept
{
    // safety_ 只有在已觀察人工釋放時才可能解除自身 fault；PI 一律清零。
    safety_.reset();
    command_slew_.reset();
    reset_pi();
}

void controller::reset_pi() noexcept
{
    for (velocity_pi& pi : pi_)
    {
        pi.reset();
    }
}

} // namespace vehicle::chassis
