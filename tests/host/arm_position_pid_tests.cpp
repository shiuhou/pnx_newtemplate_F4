#include "vehicle/arm/control/position_pid.hpp"

// 可執行規格：J1 位置外環只產生目標角速度；非法輸入一律歸零。

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{

using vehicle::arm::position_pid;
using vehicle::arm::position_pid_config;
using vehicle::arm::valid;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool near(float actual, float expected) noexcept
{
    return std::fabs(actual - expected) < 0.0001F;
}

void test_validates_config() noexcept
{
    require(valid({1.0F, 0.0F, 5.0F}));
    require(valid({0.0F, 0.5F, 5.0F}));
    require(valid({1.0F, 0.5F, 5.0F}));

    require(!valid({0.0F, 0.0F, 5.0F}));
    require(!valid({-1.0F, 0.0F, 5.0F}));
    require(!valid({1.0F, -0.1F, 5.0F}));
    require(!valid({1.0F, 0.0F, 0.0F}));
    require(!valid({1.0F, 0.0F, -1.0F}));

    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();
    require(!valid({nan, 0.0F, 5.0F}));
    require(!valid({1.0F, infinity, 5.0F}));
    require(!valid({1.0F, 0.0F, infinity}));
}

void test_position_error_sets_velocity_direction() noexcept
{
    position_pid controller{{2.0F, 0.0F, 5.0F}};

    require(near(controller.update(1.0F, 0.0F, 0.0F, 0.01F), 2.0F));
    require(near(controller.update(0.0F, 1.0F, 0.0F, 0.01F), -2.0F));

    // 正輸出就是 J1 往上抬起的方向；right_y > 0 的上層映射應餵出這種目標。
    require(controller.update(0.6F, 0.1F, 0.0F, 0.01F) > 0.0F);
}

void test_derivative_damps_measured_motion() noexcept
{
    position_pid controller{{2.0F, 0.5F, 10.0F}};

    require(near(controller.update(1.0F, 0.0F, 1.0F, 0.01F), 1.5F));
    require(near(controller.update(1.0F, 0.0F, 6.0F, 0.01F), -1.0F));
}

void test_clamps_output_limit() noexcept
{
    position_pid controller{{4.0F, 0.0F, 3.0F}};

    require(near(controller.update(2.0F, 0.0F, 0.0F, 0.01F), 3.0F));
    require(near(controller.update(-2.0F, 0.0F, 0.0F, 0.01F), -3.0F));
}

void test_fail_closed_reset() noexcept
{
    position_pid controller{{2.0F, 0.5F, 5.0F}};
    position_pid fresh{{2.0F, 0.5F, 5.0F}};

    require(near(controller.update(1.0F, 0.0F, 0.0F, 0.01F), 2.0F));

    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();
    require(near(controller.update(nan, 0.0F, 0.0F, 0.01F), 0.0F));
    require(near(controller.update(1.0F, infinity, 0.0F, 0.01F), 0.0F));
    require(near(controller.update(1.0F, 0.0F, 0.0F, 0.0F), 0.0F));
    require(near(controller.update(1.0F, 0.0F, 0.0F, -0.01F), 0.0F));

    controller.reset();
    const float reused = controller.update(0.5F, 0.25F, 0.2F, 0.01F);
    const float expected = fresh.update(0.5F, 0.25F, 0.2F, 0.01F);
    require(near(reused, expected));

    position_pid invalid{{0.0F, 0.0F, 5.0F}};
    require(near(invalid.update(1.0F, 0.0F, 0.0F, 0.01F), 0.0F));
}

} // namespace

int main()
{
    test_validates_config();
    test_position_error_sets_velocity_direction();
    test_derivative_damps_measured_motion();
    test_clamps_output_limit();
    test_fail_closed_reset();
    return EXIT_SUCCESS;
}
