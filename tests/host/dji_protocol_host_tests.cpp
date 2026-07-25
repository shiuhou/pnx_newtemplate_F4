#include "djiprotocol.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace protocol = motors::dji::protocol;

namespace
{

[[noreturn]] void fail() noexcept
{
    std::abort();
}

void require(bool condition) noexcept
{
    if (!condition)
    {
        fail();
    }
}

} // namespace

int main()
{
    const std::array<std::uint8_t, 8> bytes{
        0x12U, 0x34U, 0xFFU, 0x9CU, 0x80U, 0x00U, 55U, 0xA5U};
    const auto decoded =
        protocol::decode_feedback(0x203U, bytes, bytes.size());
    require(decoded.valid);
    require(decoded.motor_id == 3U);
    require(decoded.value.encoder == 0x1234U);
    require(decoded.value.speed_rpm == -100);
    require(decoded.value.current ==
            static_cast<std::int16_t>(0x8000U));
    require(decoded.value.temperature == 55U);
    require(!protocol::decode_feedback(
                 0x200U, bytes, bytes.size())
                 .valid);
    require(!protocol::decode_feedback(0x201U, bytes, 6U).valid);
    require(!protocol::decode_feedback(0x201U, bytes, 7U).valid);

    const auto low_first =
        protocol::command_slot_for_feedback(0x201U);
    const auto low_last =
        protocol::command_slot_for_feedback(0x204U);
    const auto high_first =
        protocol::command_slot_for_feedback(0x205U);
    const auto high_last =
        protocol::command_slot_for_feedback(0x208U);
    require(low_first.valid && low_first.frame_id == 0x200U &&
            low_first.slot == 0U);
    require(low_last.valid && low_last.frame_id == 0x200U &&
            low_last.slot == 3U);
    require(high_first.valid && high_first.frame_id == 0x1FFU &&
            high_first.slot == 0U);
    require(high_last.valid && high_last.frame_id == 0x1FFU &&
            high_last.slot == 3U);
    require(!protocol::command_slot_for_feedback(0x209U).valid);

    auto frame = protocol::zero_command(0x200U);
    require(protocol::pack_current(frame, 0x202U, -2));
    require(frame.id == 0x200U);
    require(frame.data[0] == 0U && frame.data[1] == 0U);
    require(frame.data[2] == 0xFFU);
    require(frame.data[3] == 0xFEU);
    require(frame.data[4] == 0U && frame.data[7] == 0U);

    auto high_frame = protocol::zero_command(0x1FFU);
    require(protocol::pack_current(high_frame, 0x208U, 0x1234));
    require(high_frame.data[6] == 0x12U);
    require(high_frame.data[7] == 0x34U);

    const auto zero = protocol::zero_command(0x200U);
    for (const std::uint8_t value : zero.data)
    {
        require(value == 0U);
    }

    require(protocol::clamp_current(600, 500) == 500);
    require(protocol::clamp_current(-600, 500) == -500);
    require(protocol::clamp_current(123, 500) == 123);
    require(protocol::clamp_current(123, 0) == 0);
    require(protocol::model_current_limit(protocol::model::m2006) ==
            10000);
    require(protocol::model_current_limit(protocol::model::m3508) ==
            16384);
    require(protocol::model_current_limit(protocol::model::gm6020) ==
            15000);

    require(protocol::encoder_turn_delta(100U, 8000U) == 1);
    require(protocol::encoder_turn_delta(8000U, 100U) == -1);
    require(protocol::encoder_turn_delta(4100U, 100U) == 0);

    require(!protocol::timed_out(1000U, 950U, 100U));
    require(protocol::timed_out(1050U, 950U, 100U));
    require(protocol::timed_out(1051U, 950U, 100U));
    require(protocol::timed_out(0x10U, 0xFFFFFFF0U, 32U));
    require(protocol::timed_out(0x11U, 0xFFFFFFF0U, 32U));
    require(!protocol::online(false, 1000U, 999U, 100U));
    require(protocol::online(true, 1000U, 950U, 100U));
    require(!protocol::online(true, 1051U, 950U, 100U));
    return 0;
}
