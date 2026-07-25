#include "djiprotocol.hpp"

#include <array>
#include <cstdint>

namespace protocol = motors::dji::protocol;

constexpr std::array<std::uint8_t, 8> feedback_bytes{
    0x12, 0x34, 0xFF, 0x9C, 0x80, 0x00, 55, 0xA5};
constexpr auto decoded =
    protocol::decode_feedback(0x203U, feedback_bytes, feedback_bytes.size());
static_assert(decoded.valid);
static_assert(decoded.motor_id == 3U);
static_assert(decoded.value.encoder == 0x1234U);
static_assert(decoded.value.speed_rpm == -100);
static_assert(decoded.value.current == static_cast<std::int16_t>(0x8000U));
static_assert(decoded.value.temperature == 55U);

constexpr auto bad_id =
    protocol::decode_feedback(0x200U, feedback_bytes, feedback_bytes.size());
static_assert(!bad_id.valid);
constexpr auto short_frame =
    protocol::decode_feedback(0x201U, feedback_bytes, 6U);
static_assert(!short_frame.valid);
constexpr auto seven_byte_frame =
    protocol::decode_feedback(0x201U, feedback_bytes, 7U);
static_assert(!seven_byte_frame.valid);

constexpr auto low_first = protocol::command_slot_for_feedback(0x201U);
constexpr auto low_last = protocol::command_slot_for_feedback(0x204U);
constexpr auto high_first = protocol::command_slot_for_feedback(0x205U);
constexpr auto high_last = protocol::command_slot_for_feedback(0x208U);
static_assert(low_first.valid && low_first.frame_id == 0x200U &&
              low_first.slot == 0U);
static_assert(low_last.valid && low_last.frame_id == 0x200U &&
              low_last.slot == 3U);
static_assert(high_first.valid && high_first.frame_id == 0x1FFU &&
              high_first.slot == 0U);
static_assert(high_last.valid && high_last.frame_id == 0x1FFU &&
              high_last.slot == 3U);
static_assert(!protocol::command_slot_for_feedback(0x209U).valid);

constexpr auto negative_command = [] {
    auto frame = protocol::zero_command(0x200U);
    const bool packed = protocol::pack_current(frame, 0x202U, -2);
    return packed ? frame : protocol::command_frame{};
}();
static_assert(negative_command.id == 0x200U);
static_assert(negative_command.data[0] == 0U);
static_assert(negative_command.data[1] == 0U);
static_assert(negative_command.data[2] == 0xFFU);
static_assert(negative_command.data[3] == 0xFEU);
static_assert(negative_command.data[4] == 0U);
static_assert(negative_command.data[7] == 0U);

constexpr auto positive_high_command = [] {
    auto frame = protocol::zero_command(0x1FFU);
    const bool packed = protocol::pack_current(frame, 0x208U, 0x1234);
    return packed ? frame : protocol::command_frame{};
}();
static_assert(positive_high_command.data[6] == 0x12U);
static_assert(positive_high_command.data[7] == 0x34U);

constexpr auto all_zero = protocol::zero_command(0x200U);
static_assert(all_zero.data[0] == 0U && all_zero.data[1] == 0U &&
              all_zero.data[2] == 0U && all_zero.data[3] == 0U &&
              all_zero.data[4] == 0U && all_zero.data[5] == 0U &&
              all_zero.data[6] == 0U && all_zero.data[7] == 0U);

static_assert(protocol::clamp_current(600, 500) == 500);
static_assert(protocol::clamp_current(-600, 500) == -500);
static_assert(protocol::clamp_current(123, 500) == 123);
static_assert(protocol::clamp_current(123, 0) == 0);
static_assert(protocol::model_current_limit(protocol::model::m2006) == 10000);
static_assert(protocol::model_current_limit(protocol::model::m3508) == 16384);
static_assert(protocol::model_current_limit(protocol::model::gm6020) == 15000);
static_assert(protocol::encoder_turn_delta(100U, 8000U) == 1);
static_assert(protocol::encoder_turn_delta(8000U, 100U) == -1);
static_assert(protocol::encoder_turn_delta(4100U, 100U) == 0);

static_assert(!protocol::timed_out(1000U, 950U, 100U));
static_assert(protocol::timed_out(1050U, 950U, 100U));
static_assert(protocol::timed_out(1051U, 950U, 100U));
static_assert(protocol::timed_out(0x10U, 0xFFFFFFF0U, 32U));
static_assert(protocol::timed_out(0x11U, 0xFFFFFFF0U, 32U));
static_assert(!protocol::online(false, 1000U, 999U, 100U));
static_assert(protocol::online(true, 1000U, 950U, 100U));
static_assert(!protocol::online(true, 1051U, 950U, 100U));
