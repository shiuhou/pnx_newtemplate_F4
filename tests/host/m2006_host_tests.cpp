#include "bsp_can.hpp"
#include "djimotorhandler.hpp"
#include "djimotors.hpp"
#include "fake_can_backend.hpp"

#include <array>
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

} // namespace

int main()
{
    host_test::fake_can::reset();

    motors::config config{};
    config.can_bus = bsp::can::bus::can1;
    config.can_type = bsp::can::bus_type::classic;
    config.can_id = 0x203U;
    motors::m2006 motor{config};
    auto& handler = motors::djimotorhandler::instance();
    require(handler.register_motor(motor));

    bsp::can::rx_frame feedback{};
    feedback.id = 0x203U;
    feedback.len = 7U;
    feedback.data[0] = 0x12U;
    feedback.data[1] = 0x34U;
    feedback.data[2] = 0xFFU;
    feedback.data[3] = 0x9CU;
    feedback.data[4] = 0x00U;
    feedback.data[5] = 0x7BU;
    feedback.data[6] = 42U;
    bsp::can::detail::rx_from_isr(
        bsp::can::bus::can1, feedback, 10U);

    require(motor.raw.ecd == 0x1234U);
    require(motor.raw.speed_rpm == -100);
    require(motor.fdb.current == 123.0F);
    require(motor.fdb.temperature == 42.0F);

    motor.set_current(500);
    handler.send_control();
    const auto tx = host_test::fake_can::last_transmit();
    require(tx.bus == bsp::can::bus::can1);
    require(tx.id == 0x200U);
    require(tx.len == 8U);
    constexpr std::array<std::uint8_t, 8U> expected{
        0U, 0U, 0U, 0U, 0x01U, 0xF4U, 0U, 0U};
    require(tx.data == expected);
    return EXIT_SUCCESS;
}
