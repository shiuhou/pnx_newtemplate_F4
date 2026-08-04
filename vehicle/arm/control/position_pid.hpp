#pragma once

namespace vehicle::arm
{

// J1 位置外環：輸出的是目標角速度，不是 CAN raw current。
// 符號約定：正角度 / 正角速度代表 J1 往上抬起；因此 right_y > 0 的遙控輸入
// 在上層應轉成更大的 target_position_rad。
struct position_pid_config {
    float kp{};                          // 單位 (rad/s)/rad。
    float kd{};                          // 單位 (rad/s)/(rad/s)。
    float target_velocity_limit_rad_s{}; // 外環輸出的絕對值上限。
};

// 檢查增益與目標速度上限是否有限且落在可工作的安全契約內。
bool valid(const position_pid_config& config) noexcept;

class position_pid {
public:
    explicit position_pid(position_pid_config config) noexcept;

    // 以位置誤差與量測角速度更新外環，回傳交給速度內環的目標角速度。
    float update(float target_position_rad,
                 float measured_position_rad,
                 float measured_velocity_rad_s,
                 float dt_s) noexcept;
    // 停止、失效或重新解鎖時清掉上一筆輸出，避免上層沿用舊狀態。
    void reset() noexcept;

private:
    position_pid_config config_{};
    float last_output_rad_s_{};
    bool config_valid_{};
};

} // namespace vehicle::arm
