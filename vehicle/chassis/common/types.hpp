#pragma once

#include <array>
#include <cstdint>

namespace vehicle::chassis
{

// 車輛層所有四元素陣列都使用這個邏輯輪序。
// 注意：實體 C610 ID 順時針是 1/2/3/4，但陣列順序是 ID 1/2/4/3。
enum class wheel : std::uint8_t {
    front_left,  // FL：左前，C610 ID 1，feedback 0x201。
    front_right, // FR：右前，C610 ID 2，feedback 0x202。
    rear_left,   // RL：左後，C610 ID 4，feedback 0x204。
    rear_right,  // RR：右後，C610 ID 3，feedback 0x203。
};

// 底盤中心在車體座標系中的期望速度。
struct body_velocity {
    float vx_mps{};     // +x 向車頭，單位 m/s。
    float vy_mps{};     // +y 向車體左側，單位 m/s。
    float yaw_rad_s{};  // 正值為俯視逆時針，單位 rad/s。
};

// 四輪角速度容器。它可以表示目標或量測值，語意由變數名稱決定。
struct wheel_vector {
    // 固定為 FL/FR/RL/RR；這裡使用 M2006 減速箱輸出端而非轉子端 rad/s。
    std::array<float, 4U> rad_s{};
};

// X 型麥輪逆運動學所需的實車幾何。
struct geometry {
    float wheel_radius_m{};     // 輪子有效接觸半徑，單位 m。
    float half_wheelbase_m{};   // 前後輪中心距的一半，單位 m。
    float half_track_m{};       // 左右輪中心距的一半，單位 m。
    float max_wheel_rad_s{};    // 等比例限幅後的單輪最大輸出端 rad/s。
};

} // namespace vehicle::chassis
