#pragma once

#include "vehicle/chassis/types.hpp"

#include <optional>

namespace vehicle::chassis
{

std::optional<wheel_vector> inverse_kinematics(
    const body_velocity& command, const geometry& g) noexcept;

} // namespace vehicle::chassis
