#include "vehicle/arm/runtime/arm_config.hpp"

// 可執行規格：arm runtime 設定只接受有限值、正的 J1 手動位置斜率與正控制週期。

#include <cstdlib>
#include <limits>

namespace
{

using vehicle::arm::configuration;
using vehicle::arm::position_pid_config;
using vehicle::arm::valid;

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

configuration healthy_config() noexcept
{
    return {
        position_pid_config{2.0F, 0.5F, 5.0F},
        1.5F,
        0.005F,
    };
}

void test_accepts_healthy_config() noexcept
{
    require(valid(healthy_config()));
}

void test_rejects_invalid_position_pid_config() noexcept
{
    auto config = healthy_config();
    config.j1_position = {0.0F, 0.0F, 5.0F};
    require(!valid(config));

    config = healthy_config();
    config.j1_position = {-1.0F, 0.0F, 5.0F};
    require(!valid(config));

    config = healthy_config();
    config.j1_position = {1.0F, -0.1F, 5.0F};
    require(!valid(config));

    config = healthy_config();
    config.j1_position = {1.0F, 0.0F, 0.0F};
    require(!valid(config));
}

void test_rejects_invalid_manual_position_rate() noexcept
{
    auto config = healthy_config();
    config.j1_manual_position_rate_rad_per_s = 0.0F;
    require(!valid(config));

    config = healthy_config();
    config.j1_manual_position_rate_rad_per_s = -1.0F;
    require(!valid(config));
}

void test_rejects_invalid_control_period() noexcept
{
    auto config = healthy_config();
    config.control_period_s = 0.0F;
    require(!valid(config));

    config = healthy_config();
    config.control_period_s = -0.005F;
    require(!valid(config));
}

void test_rejects_non_finite_values() noexcept
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();

    auto config = healthy_config();
    config.j1_position.kp = nan;
    require(!valid(config));

    config = healthy_config();
    config.j1_manual_position_rate_rad_per_s = infinity;
    require(!valid(config));

    config = healthy_config();
    config.control_period_s = nan;
    require(!valid(config));
}

} // namespace

int main()
{
    test_accepts_healthy_config();
    test_rejects_invalid_position_pid_config();
    test_rejects_invalid_manual_position_rate();
    test_rejects_invalid_control_period();
    test_rejects_non_finite_values();
    return EXIT_SUCCESS;
}
