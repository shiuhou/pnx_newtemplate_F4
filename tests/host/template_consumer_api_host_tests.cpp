#include "bsp_can.hpp"
#include "bsp_pwm.hpp"
#include "bsp_spi.hpp"
#include "bsp_usart.hpp"
#include "motor.hpp"

#include <type_traits>

static_assert(bsp::can::bus::fdcan1 == bsp::can::bus::can1);
static_assert(bsp::can::bus::fdcan2 == bsp::can::bus::can2);
static_assert(
    motors::config{}.can_bus == bsp::can::bus::fdcan1);

static_assert(bsp::spi::bus::spi2 == bsp::spi::bus{0U});
static_assert(bsp::spi::bus::spi6 == bsp::spi::bus{1U});
static_assert(
    bsp::spi::cs::bmi088_acc == bsp::spi::chip_select{0U});
static_assert(
    bsp::spi::cs::bmi088_gyro == bsp::spi::chip_select{1U});

using legacy_cs_set =
    void (*)(bsp::spi::cs, bool) noexcept;
static_assert(std::is_same_v<
              decltype(&bsp::spi::cs_set), legacy_cs_set>);

static_assert(
    bsp::pwm::channel::tim3_ch4 != bsp::pwm::channel{0U});
static_assert(
    bsp::pwm::channel::tim12_ch2 != bsp::pwm::channel{0U});

using legacy_set_period =
    void (*)(bsp::pwm::channel, float) noexcept;
static_assert(std::is_same_v<
              decltype(&bsp::pwm::set_period), legacy_set_period>);

using template_start_rx = types::status (*)(
    bsp::usart::port, std::uint8_t*, std::size_t,
    bsp::usart::rx_handler, void*, bsp::usart::notify_handler,
    void*, bsp::usart::rx_delivery);
static_assert(std::is_same_v<
              decltype(&bsp::usart::start_rx_to_idle),
              template_start_rx>);

int main()
{
    return 0;
}
