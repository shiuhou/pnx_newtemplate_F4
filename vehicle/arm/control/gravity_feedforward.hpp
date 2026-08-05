#pragma once

#include <cstdint>

namespace vehicle::arm
{

float gravity_current_raw(float position_rad,
                          float amplitude_raw,
                          float phase_rad,
                          float bias_raw) noexcept;

std::int16_t combine_current_raw(std::int16_t feedback_raw,
                                 float feedforward_raw,
                                 float current_limit_raw) noexcept;

} // namespace vehicle::arm
