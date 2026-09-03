#include "vehicle/rfid/protocol.hpp"
#include "vehicle/rfid/reader_core.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace
{

using vehicle::rfid::link_state;
using vehicle::rfid::reader_core;
using vehicle::rfid::protocol::command;

constexpr std::array<std::uint8_t, 12U> vendor_uid{
    0x04U, 0x0CU, 0x02U, 0x20U, 0x00U, 0x04U,
    0x00U, 0x45U, 0x96U, 0xB7U, 0x8AU, 0x3FU,
};
constexpr std::array<std::uint8_t, 8U> vendor_b1_query{
    0x02U, 0x08U, 0xB1U, 0x20U, 0x00U, 0x00U, 0x00U, 0x64U,
};
constexpr std::array<std::uint8_t, 9U> vendor_b1_response{
    0x02U, 0x09U, 0xB1U, 0x20U, 0x00U,
    0x02U, 0x00U, 0x00U, 0x67U,
};
constexpr std::array<std::uint8_t, 8U> vendor_b8_frame{
    0x02U, 0x08U, 0xB8U, 0x20U, 0x00U, 0x00U, 0x00U, 0x6DU,
};

void require_impl(bool condition, int line) noexcept
{
    if (!condition)
    {
        std::fprintf(stderr, "RFID test requirement failed at line %d\n", line);
        std::abort();
    }
}

#define require(condition) require_impl((condition), __LINE__)

template <typename Left, typename Right>
void require_equal(const Left& left, const Right& right) noexcept
{
    require(left == right);
}

struct parsed_stream
{
    std::array<vehicle::rfid::protocol::frame, 8U> frames{};
    std::size_t frame_count{};
    std::uint32_t checksum_errors{};
    std::uint32_t frame_errors{};
};

template <std::size_t Size>
void feed_parser(vehicle::rfid::protocol::parser& parser,
                 const std::array<std::uint8_t, Size>& bytes,
                 parsed_stream& output) noexcept
{
    for (const std::uint8_t byte : bytes)
    {
        parser.push(byte);
        for (;;)
        {
            vehicle::rfid::protocol::frame next{};
            const auto result = parser.next(next);
            if (result == vehicle::rfid::protocol::parse_result::none)
            {
                break;
            }
            if (result == vehicle::rfid::protocol::parse_result::frame)
            {
                require(output.frame_count < output.frames.size());
                output.frames[output.frame_count++] = next;
            }
            else if (result ==
                     vehicle::rfid::protocol::parse_result::checksum_error)
            {
                ++output.checksum_errors;
            }
            else
            {
                ++output.frame_errors;
            }
        }
    }
}

void test_vendor_golden_vectors() noexcept
{
    require(vehicle::rfid::protocol::checksum(
                vendor_uid.data(), vendor_uid.size() - 1U) == 0x3FU);
    require_equal(vehicle::rfid::protocol::make_query(
                      command::b1, 0x20U), vendor_b1_query);
    require_equal(vehicle::rfid::protocol::make_query(
                      command::b8, 0x20U), vendor_b8_frame);

    vehicle::rfid::protocol::parser parser{};
    parsed_stream parsed{};
    feed_parser(parser, vendor_uid, parsed);
    feed_parser(parser, vendor_b1_response, parsed);
    feed_parser(parser, vendor_b8_frame, parsed);
    require(parsed.frame_count == 3U);
    require(parsed.checksum_errors == 0U);
    require(parsed.frame_errors == 0U);

    vehicle::rfid::protocol::uid_report uid{};
    require(vehicle::rfid::protocol::decode_uid(
        parsed.frames[0], 0x20U, uid));
    require_equal(uid.card_type,
                  (std::array<std::uint8_t, 2U>{0x04U, 0x00U}));
    require_equal(uid.uid,
                  (std::array<std::uint8_t, 4U>{
                      0x45U, 0x96U, 0xB7U, 0x8AU}));

    vehicle::rfid::protocol::work_mode mode{};
    require(vehicle::rfid::protocol::decode_b1(
        parsed.frames[1], 0x20U, mode));
    require(mode.mode == 0x02U);
    require(mode.block == 0x00U);
    require(mode.upload == 0x00U);

    vehicle::rfid::protocol::read_behavior behavior{};
    require(vehicle::rfid::protocol::decode_b8(
        parsed.frames[2], 0x20U, behavior));
    require(behavior.mode == 0x00U);
}

void test_stream_resynchronization() noexcept
{
    vehicle::rfid::protocol::parser parser{};
    parsed_stream parsed{};
    constexpr std::array<std::uint8_t, 7U> garbage{
        0xFFU, 0x00U, 0x02U, 0x40U, 0x04U, 0x03U, 0x99U,
    };
    feed_parser(parser, garbage, parsed);
    feed_parser(parser, vendor_uid, parsed);
    feed_parser(parser, vendor_b1_response, parsed);
    require(parsed.frame_count == 2U);
    require(parsed.frame_errors != 0U);

    auto bad_checksum = vendor_uid;
    bad_checksum.back() ^= 0x01U;
    feed_parser(parser, bad_checksum, parsed);
    feed_parser(parser, vendor_uid, parsed);
    require(parsed.frame_count == 3U);
    require(parsed.checksum_errors == 1U);
}

void test_split_frame_waits_for_completion() noexcept
{
    vehicle::rfid::protocol::parser parser{};
    parsed_stream parsed{};
    std::array<std::uint8_t, 5U> first{};
    std::array<std::uint8_t, 7U> second{};
    for (std::size_t i = 0U; i < first.size(); ++i)
    {
        first[i] = vendor_uid[i];
    }
    for (std::size_t i = 0U; i < second.size(); ++i)
    {
        second[i] = vendor_uid[first.size() + i];
    }
    feed_parser(parser, first, parsed);
    require(parsed.frame_count == 0U);
    feed_parser(parser, second, parsed);
    require(parsed.frame_count == 1U);
}

void complete_startup(reader_core& reader, std::uint32_t start_tick) noexcept
{
    reader.start(start_tick);
    require(!reader.poll(start_tick + 999U).has_value());
    const auto b1 = reader.poll(start_tick + 1000U);
    require(b1.has_value());
    require(b1->kind == command::b1);
    require_equal(b1->bytes, vendor_b1_query);
    reader.feed(vendor_b1_response.data(), vendor_b1_response.size(),
                start_tick + 1010U);
    require(!reader.poll(start_tick + 1129U).has_value());
    const auto b8 = reader.poll(start_tick + 1130U);
    require(b8.has_value());
    require(b8->kind == command::b8);
    reader.feed(vendor_b8_frame.data(), vendor_b8_frame.size(),
                start_tick + 1140U);
    require(reader.snapshot().link == link_state::ready);
}

void test_async_uid_and_startup_verification() noexcept
{
    reader_core reader{0x20U};
    reader.start(0U);
    const auto b1 = reader.poll(1000U);
    require(b1.has_value());
    reader.feed(vendor_uid.data(), vendor_uid.size(), 1001U);
    require(reader.snapshot().event_count == 0U);
    reader.feed(vendor_b1_response.data(), vendor_b1_response.size(), 1010U);
    const auto b8 = reader.poll(1130U);
    require(b8.has_value());
    reader.feed(vendor_b8_frame.data(), vendor_b8_frame.size(), 1140U);
    reader.feed(vendor_uid.data(), vendor_uid.size(), 1141U);

    const auto state = reader.snapshot();
    require(state.link == link_state::ready);
    require(state.event_count == 1U);
    require(state.last_uid_tick == 1141U);
    require_equal(state.last_uid,
                  (std::array<std::uint8_t, 4U>{
                      0x45U, 0x96U, 0xB7U, 0x8AU}));
}

void test_config_mismatch_is_not_retried() noexcept
{
    reader_core reader{0x20U};
    reader.start(0U);
    require(reader.poll(1000U).has_value());
    auto mismatch = vendor_b1_response;
    mismatch[5] = 0x01U;
    mismatch.back() = vehicle::rfid::protocol::checksum(
        mismatch.data(), mismatch.size() - 1U);
    reader.feed(mismatch.data(), mismatch.size(), 1010U);
    require(reader.snapshot().link == link_state::config_mismatch);
    require(!reader.poll(5000U).has_value());
}

void test_timeout_retries_and_recovery() noexcept
{
    reader_core reader{0x20U};
    reader.start(0U);
    require(reader.poll(1000U).has_value());
    require(!reader.poll(1300U).has_value());
    require(reader.poll(1420U).has_value());
    require(!reader.poll(1720U).has_value());
    require(reader.poll(1840U).has_value());
    require(!reader.poll(2140U).has_value());
    require(reader.snapshot().link == link_state::timeout);
    require(reader.snapshot().timeout_errors == 3U);

    const auto retry = reader.poll(3140U);
    require(retry.has_value());
    require(retry->kind == command::b1);
    require(reader.snapshot().link == link_state::verifying);
    reader.feed(vendor_b1_response.data(), vendor_b1_response.size(), 3150U);
    require(reader.poll(3270U).has_value());
    reader.feed(vendor_b8_frame.data(), vendor_b8_frame.size(), 3280U);
    require(reader.snapshot().link == link_state::ready);
}

void test_health_query_keeps_uid_delivery_live() noexcept
{
    reader_core reader{0x20U};
    complete_startup(reader, 0U);
    const auto health = reader.poll(3140U);
    require(health.has_value());
    require(health->kind == command::b1);
    reader.feed(vendor_uid.data(), vendor_uid.size(), 3141U);
    require(reader.snapshot().event_count == 1U);
    reader.feed(vendor_b1_response.data(), vendor_b1_response.size(), 3150U);
    require(reader.snapshot().link == link_state::ready);
}

void test_health_retry_remains_a_lightweight_b1_query() noexcept
{
    reader_core reader{0x20U};
    complete_startup(reader, 0U);
    require(reader.poll(3140U).has_value());
    require(!reader.poll(3440U).has_value());
    const auto retry = reader.poll(3560U);
    require(retry.has_value());
    require(retry->kind == command::b1);
    reader.feed(vendor_b1_response.data(), vendor_b1_response.size(), 3570U);
    require(reader.snapshot().link == link_state::ready);
    require(!reader.poll(3690U).has_value());
    const auto next_health = reader.poll(5570U);
    require(next_health.has_value());
    require(next_health->kind == command::b1);
}

void test_parser_errors_are_observable() noexcept
{
    reader_core reader{0x20U};
    complete_startup(reader, 0U);
    auto bad_checksum = vendor_uid;
    bad_checksum.back() ^= 0x01U;
    reader.feed(bad_checksum.data(), bad_checksum.size(), 1200U);
    reader.record_overflow();
    const auto state = reader.snapshot();
    require(state.checksum_errors == 1U);
    require(state.overflow_errors == 1U);
    require(state.event_count == 0U);
}

void test_bounded_random_stream() noexcept
{
    vehicle::rfid::protocol::parser parser{};
    parsed_stream parsed{};
    std::uint32_t value = 0xC001D00DU;
    for (std::size_t i = 0U; i < 10000U; ++i)
    {
        value = value * 1664525U + 1013904223U;
        const std::array<std::uint8_t, 1U> byte{
            static_cast<std::uint8_t>(value >> 24U)};
        feed_parser(parser, byte, parsed);
        parsed.frame_count = 0U;
    }
    feed_parser(parser, vendor_uid, parsed);
    require(parsed.frame_count == 1U);
}

} // namespace

int main()
{
    test_vendor_golden_vectors();
    test_stream_resynchronization();
    test_split_frame_waits_for_completion();
    test_async_uid_and_startup_verification();
    test_config_mismatch_is_not_retried();
    test_timeout_retries_and_recovery();
    test_health_query_keeps_uid_delivery_live();
    test_health_retry_remains_a_lightweight_b1_query();
    test_parser_errors_are_observable();
    test_bounded_random_stream();
    return EXIT_SUCCESS;
}
