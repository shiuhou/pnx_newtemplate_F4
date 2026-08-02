#include "vehicle/chassis/control/kinematics.hpp"

// 可執行規格：固定車體座標與 FL/FR/RL/RR 輪序的 X 麥輪逆解及等比例限速。

#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::chassis::body_velocity;
using vehicle::chassis::geometry;
using vehicle::chassis::inverse_kinematics;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool equal(const std::array<float, 4U>& actual,
           const std::array<float, 4U>& expected) noexcept
{
    constexpr float tolerance = 0.0001F;
    for (std::size_t index = 0U; index < actual.size(); ++index)
    {
        if (!std::isfinite(actual[index]) ||
            !std::isfinite(expected[index]) ||
            std::fabs(actual[index] - expected[index]) > tolerance)
        {
            return false;
        }
    }
    return true;
}

void require_invalid(const body_velocity& command,
                     const geometry& g) noexcept
{
    require(!inverse_kinematics(command, g).has_value());
}

} // namespace

int main()
{
    using namespace vehicle::chassis;

    constexpr geometry g{0.10F, 0.20F, 0.15F, 100.0F};
    require(equal(inverse_kinematics({1.0F, 0.0F, 0.0F}, g)->rad_s,
                  {10.0F, 10.0F, 10.0F, 10.0F}));
    require(equal(inverse_kinematics({0.0F, 1.0F, 0.0F}, g)->rad_s,
                  {-10.0F, 10.0F, 10.0F, -10.0F}));
    require(equal(inverse_kinematics({0.0F, 0.0F, 1.0F}, g)->rad_s,
                  {-3.5F, 3.5F, -3.5F, 3.5F}));
    require(!inverse_kinematics({}, geometry{}).has_value());

    const auto saturated = inverse_kinematics({15.0F, 5.0F, 0.0F}, g);
    require(saturated.has_value());
    require(equal(saturated->rad_s, {50.0F, 100.0F, 100.0F, 50.0F}));

    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    require(!equal({nan, 0.0F, 0.0F, 0.0F},
                   {0.0F, 0.0F, 0.0F, 0.0F}));
    require_invalid({infinity, 0.0F, 0.0F}, g);
    require_invalid({0.0F, nan, 0.0F}, g);
    require_invalid({0.0F, 0.0F, infinity}, g);
    require_invalid({}, {nan, 0.20F, 0.15F, 100.0F});
    require_invalid({}, {0.10F, infinity, 0.15F, 100.0F});
    require_invalid({}, {0.10F, 0.20F, nan, 100.0F});
    require_invalid({}, {0.10F, 0.20F, 0.15F, infinity});
    require_invalid({}, {0.0F, 0.20F, 0.15F, 100.0F});
    require_invalid({}, {-0.10F, 0.20F, 0.15F, 100.0F});
    require_invalid({}, {0.10F, -0.20F, 0.15F, 100.0F});
    require_invalid({}, {0.10F, 0.20F, -0.15F, 100.0F});
    require_invalid({}, {0.10F, 0.20F, 0.15F, 0.0F});
    require_invalid({}, {0.10F, 0.20F, 0.15F, -100.0F});
    return EXIT_SUCCESS;
}
