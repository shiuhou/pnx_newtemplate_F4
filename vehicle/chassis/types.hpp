#pragma once

#include <array>
#include <cstdint>

namespace vehicle::chassis
{

enum class wheel : std::uint8_t {
    front_left,
    front_right,
    rear_left,
    rear_right,
};

struct body_velocity {
    float vx_mps{};
    float vy_mps{};
    float yaw_rad_s{};
};

struct wheel_vector {
    std::array<float, 4U> rad_s{};
};

struct geometry {
    float wheel_radius_m{};
    float half_wheelbase_m{};
    float half_track_m{};
    float max_wheel_rad_s{};
};

} // namespace vehicle::chassis
