#include "bsp_pwm.hpp"
#include "fake_pwm.hpp"
#include "pwm_channels.hpp"

#include <cstdlib>

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

constexpr bsp::pwm::channel c2 = board::pwm::servo_c2;
constexpr bsp::pwm::channel pe9 = board::pwm::servo_pe9;
constexpr bsp::pwm::channel pe13 = board::pwm::servo_pe13;
constexpr bsp::pwm::channel pe14 = board::pwm::servo_pe14;

// 舵機 50Hz 時基。
constexpr std::uint32_t servo_period_us = 20000U;

void single_channel_contract()
{
    host_test::fake_pwm::reset();
    constexpr bsp::pwm::channel invalid{board::pwm::servo_channel_count};

    require(bsp::pwm::start(invalid) == types::status::not_configured);
    require(bsp::pwm::set_period_us(c2, 0U) ==
            types::status::invalid_arg);
    require(bsp::pwm::set_pulse_us(c2, servo_period_us + 1U) ==
            types::status::invalid_arg);

    require(bsp::pwm::set_period_us(c2, servo_period_us) ==
            types::status::ok);
    bsp::pwm::set_period(c2, 0.010F);
    require(host_test::fake_pwm::period_us() == 10000U);
    bsp::pwm::set_period(bsp::pwm::channel::tim3_ch4, 0.020F);
    require(host_test::fake_pwm::period_us() == 10000U);
    require(bsp::pwm::set_period_us(c2, servo_period_us) ==
            types::status::ok);
    require(bsp::pwm::set_pulse_us(c2, 1500U) == types::status::ok);
    require(bsp::pwm::start(c2) == types::status::ok);
    require(host_test::fake_pwm::started(c2));
    require(host_test::fake_pwm::period_us() == servo_period_us);
    require(host_test::fake_pwm::pulse_us(c2) == 1500U);

    require(bsp::pwm::set_duty(c2, 0.05F) == types::status::ok);
    require(host_test::fake_pwm::pulse_us(c2) == 1000U);

    require(bsp::pwm::stop(c2) == types::status::ok);
    require(!host_test::fake_pwm::started(c2));
    require(host_test::fake_pwm::pulse_us(c2) == 0U);
}

void all_servo_channels_enabled()
{
    host_test::fake_pwm::reset();
    require(bsp::pwm::is_enabled(c2));
    require(bsp::pwm::is_enabled(pe9));
    require(bsp::pwm::is_enabled(pe13));
    require(bsp::pwm::is_enabled(pe14));

    // 邏輯通道彼此不同，且 legacy template slot 仍 fail closed。
    require(c2 != pe9 && pe9 != pe13 && pe13 != pe14);
    require(!bsp::pwm::is_enabled(bsp::pwm::channel::tim3_ch4));
    require(!bsp::pwm::is_enabled(bsp::pwm::channel::tim12_ch2));
    require(!bsp::pwm::is_enabled(bsp::pwm::none));
    require(bsp::pwm::set_pulse_us(bsp::pwm::channel::tim12_ch2, 1500U) ==
            types::status::not_configured);
}

void independent_pulse_widths()
{
    host_test::fake_pwm::reset();
    require(bsp::pwm::set_period_us(c2, servo_period_us) ==
            types::status::ok);

    // 三路舵機各給不同脈寬：最小／中位／最大。
    require(bsp::pwm::set_pulse_us(pe9, 500U) == types::status::ok);
    require(bsp::pwm::set_pulse_us(pe13, 1500U) == types::status::ok);
    require(bsp::pwm::set_pulse_us(pe14, 2500U) == types::status::ok);

    require(host_test::fake_pwm::pulse_us(pe9) == 500U);
    require(host_test::fake_pwm::pulse_us(pe13) == 1500U);
    require(host_test::fake_pwm::pulse_us(pe14) == 2500U);

    // 未設定的通道保持零輸出，不被其他通道帶動。
    require(host_test::fake_pwm::pulse_us(c2) == 0U);

    // 覆寫一路不影響其餘各路。
    require(bsp::pwm::set_pulse_us(pe13, 1000U) == types::status::ok);
    require(host_test::fake_pwm::pulse_us(pe9) == 500U);
    require(host_test::fake_pwm::pulse_us(pe13) == 1000U);
    require(host_test::fake_pwm::pulse_us(pe14) == 2500U);

    // set_duty 對共用週期換算，且只作用在目標通道。
    require(bsp::pwm::set_duty(pe14, 0.075F) == types::status::ok);
    require(host_test::fake_pwm::pulse_us(pe14) == 1500U);
    require(host_test::fake_pwm::pulse_us(pe9) == 500U);
    require(host_test::fake_pwm::pulse_us(pe13) == 1000U);
}

void shared_time_base()
{
    host_test::fake_pwm::reset();

    // 任一通道設定週期都套用到共用時基 → 三路同為 50Hz。
    require(bsp::pwm::set_period_us(pe13, servo_period_us) ==
            types::status::ok);
    require(host_test::fake_pwm::period_us() == servo_period_us);

    require(bsp::pwm::set_pulse_us(pe9, servo_period_us) ==
            types::status::ok);
    require(host_test::fake_pwm::pulse_us(pe9) == servo_period_us);

    // 縮短共用週期不得讓任一路既有脈寬超過新週期；拒絕時狀態不變。
    require(bsp::pwm::set_period_us(pe14, 10000U) ==
            types::status::invalid_arg);
    require(host_test::fake_pwm::period_us() == servo_period_us);
    require(host_test::fake_pwm::pulse_us(pe9) == servo_period_us);

    // 先把既有脈寬降到新上限，才可安全縮短共用週期。
    require(bsp::pwm::set_pulse_us(pe9, 10000U) == types::status::ok);
    require(bsp::pwm::set_period_us(pe14, 10000U) == types::status::ok);
    require(host_test::fake_pwm::period_us() == 10000U);
    require(bsp::pwm::set_pulse_us(pe9, 10001U) ==
            types::status::invalid_arg);
    require(bsp::pwm::set_pulse_us(pe9, 10000U) == types::status::ok);
}

void independent_start_stop()
{
    host_test::fake_pwm::reset();
    require(bsp::pwm::set_period_us(pe9, servo_period_us) ==
            types::status::ok);
    require(bsp::pwm::start(pe9) == types::status::ok);
    require(bsp::pwm::start(pe13) == types::status::ok);
    require(bsp::pwm::start(pe14) == types::status::ok);
    require(host_test::fake_pwm::started(pe9));
    require(host_test::fake_pwm::started(pe13));
    require(host_test::fake_pwm::started(pe14));
    require(!host_test::fake_pwm::started(c2));

    require(bsp::pwm::set_pulse_us(pe9, 1500U) == types::status::ok);
    require(bsp::pwm::set_pulse_us(pe13, 1500U) == types::status::ok);

    // 停一路只清該路脈寬，其餘照跑。
    require(bsp::pwm::stop(pe13) == types::status::ok);
    require(!host_test::fake_pwm::started(pe13));
    require(host_test::fake_pwm::pulse_us(pe13) == 0U);
    require(host_test::fake_pwm::started(pe9));
    require(host_test::fake_pwm::pulse_us(pe9) == 1500U);
    require(host_test::fake_pwm::started(pe14));
}

} // namespace

int main()
{
    single_channel_contract();
    all_servo_channels_enabled();
    independent_pulse_widths();
    shared_time_base();
    independent_start_stop();
    return EXIT_SUCCESS;
}
