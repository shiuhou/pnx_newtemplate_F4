#include "djimotors.hpp"
#include "djimotorhandler.hpp"
#include "fake_can.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace
{

constexpr float pi = 3.14159265358979323846F;
constexpr float p36_gear_ratio = 36.0F;
constexpr float rated_output_rpm = 416.0F;
constexpr float no_load_output_rpm = 500.0F;

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

std::array<std::uint8_t, 8U> feedback_frame(
    std::int16_t rotor_rpm) noexcept
{
    const auto encoded = static_cast<std::uint16_t>(rotor_rpm);
    return {
        0U,
        0U,
        static_cast<std::uint8_t>(encoded >> 8U),
        static_cast<std::uint8_t>(encoded),
        0U,
        0U,
        25U,
        0U,
    };
}

void require_output_speed(motors::m2006& motor,
                          float output_rpm) noexcept
{
    const auto rotor_rpm = static_cast<std::int16_t>(
        output_rpm * p36_gear_ratio);
    const auto frame = feedback_frame(rotor_rpm);
    motor.parse_feedback(frame.data(),
                         static_cast<std::uint8_t>(frame.size()));

    const float expected_rad_s = output_rpm * 2.0F * pi / 60.0F;
    require(near(motor.get_feedback().velocity, expected_rad_s));
}

motors::config motor_config(std::uint32_t feedback_id) noexcept
{
    return {
        bsp::can::bus::can1,
        bsp::can::bus_type::classic,
        feedback_id,
        motors::mode::relax,
    };
}

void require_c610_current_frame() noexcept
{
    host_test::fake_can::reset();
    motors::m2006 id1{motor_config(0x201U)};
    motors::m2006 id2{motor_config(0x202U)};
    motors::m2006 id3{motor_config(0x203U)};
    motors::m2006 id4{motor_config(0x204U)};
    auto& handler = motors::djimotorhandler::instance();
    require(handler.register_motor(id1));
    require(handler.register_motor(id2));
    require(handler.register_motor(id3));
    require(handler.register_motor(id4));

    id1.set_current(1000);
    id2.set_current(-1000);
    id3.set_current(10000);
    id4.set_current(-10000);
    handler.send_control();

    const auto sent = host_test::fake_can::last_transmit();
    require(host_test::fake_can::transmit_calls() == 1U);
    require(sent.bus == bsp::can::bus::can1);
    require(sent.id == 0x200U);
    require(sent.len == 8U);
    require((sent.data == std::array<std::uint8_t, 8U>{
                              0x03U, 0xE8U,
                              0xFCU, 0x18U,
                              0x27U, 0x10U,
                              0xD8U, 0xF0U,
                          }));
}

} // namespace

int main()
{
    // DJI M2006 P36 v1.0 (2019-03), pages 8-9: 36:1 gearbox,
    // 416 rpm rated output speed and 500 rpm no-load output speed with C610.
    const motors::config config{
        bsp::can::bus::can1,
        bsp::can::bus_type::classic,
        0x201U,
        motors::mode::relax,
    };
    motors::m2006 motor{config};

    require_output_speed(motor, rated_output_rpm);
    require_output_speed(motor, no_load_output_rpm);
    require_output_speed(motor, -rated_output_rpm);
    // C610 v1.0, page 8: IDs 1-4 share standard frame 0x200,
    // use signed big-endian 16-bit slots, and raw +/-10000 means +/-10 A.
    require_c610_current_frame();
    return EXIT_SUCCESS;
}
