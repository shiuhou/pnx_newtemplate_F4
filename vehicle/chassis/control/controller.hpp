#pragma once

#include "vehicle/chassis/control/kinematics.hpp"
#include "vehicle/chassis/control/manual_control.hpp"
#include "vehicle/chassis/control/safety_gate.hpp"
#include "vehicle/chassis/control/velocity_pi.hpp"

#include <array>
#include <cstdint>

namespace vehicle::chassis
{

// controller 執行一次四輪閉環所需的純資料設定；不包含 ThreadX、CAN 或 UART。
struct controller_configuration {
    // 實車尺寸與單輪角速度上限。
    ::vehicle::chassis::geometry geometry;
    // DR16 到 vx/vy/yaw 的速度限制與軸方向。
    manual_limits manual;
    // 將車體輪正方向映射到各馬達 encoder 正方向；順序 FL/FR/RL/RR。
    std::array<float, 4U> motor_direction;
    // 四顆輪速 PI 共用的增益與 raw-current 上限。
    velocity_pi_config pi;
};

// controller 建構前的總資料檢查；不讀硬體狀態。
bool valid(const controller_configuration& config) noexcept;

// 一次 controller::update() 的可觀測結果。
struct controller_output {
    // 麥輪逆解結果，尚未套 motor_direction；順序 FL/FR/RL/RR，單位 rad/s。
    wheel_vector wheel_target_rad_s;
    // 套用 motor_direction 並經 PI 計算後的四顆 C610 raw current。
    std::array<std::int16_t, 4U> motor_current_raw;
    // 本週期 safety gate 的狀態，runtime 仍會再套用系統級 force_zero。
    safety_state state;
};

// 純四輪控制器：DR16 映射 -> 麥輪逆解 -> safety gate -> 四個速度 PI。
class controller {
public:
    explicit controller(controller_configuration config) noexcept;

    // measured_motor_rad_s 是馬達 encoder 正方向的實際輸出端速度；
    // dt_s 是本次 PI 積分使用的秒數，目前 runtime 固定傳入 0.005。
    controller_output update(
        const manual_input& manual,
        const wheel_vector& measured_motor_rad_s,
        const safety_input& safety,
        float dt_s) noexcept;
    // 清除 safety gate 可解除的 fault 與四輪積分；不會清 runtime_policy 的故障位元。
    void reset() noexcept;

private:
    void reset_pi() noexcept;

    controller_configuration config_; // 建構時保存的車輛控制參數。
    bool config_valid_{};              // 建構時鎖定；非法設定永遠不出力。
    safety_gate safety_{};             // 控制層解鎖狀態機。
    std::array<velocity_pi, 4U> pi_;   // FL/FR/RL/RR 各自獨立的積分狀態。
};

} // namespace vehicle::chassis
