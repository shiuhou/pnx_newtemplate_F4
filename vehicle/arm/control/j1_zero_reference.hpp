#pragma once

namespace vehicle::arm
{

class j1_zero_reference {
public:
    bool capture(float physical_position_rad) noexcept;
    bool captured() const noexcept;
    float logical_position(float physical_position_rad) const noexcept;

private:
    float physical_zero_rad_{};
    bool captured_{};
};

} // namespace vehicle::arm
