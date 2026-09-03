#include "vehicle/rfid/reader_core.hpp"

namespace vehicle::rfid
{
namespace
{

constexpr std::uint32_t startup_settle_ticks = 1000U;
constexpr std::uint32_t command_timeout_ticks = 300U;
constexpr std::uint32_t command_gap_ticks = 120U;
constexpr std::uint32_t health_period_ticks = 2000U;
constexpr std::uint32_t recovery_period_ticks = 1000U;
constexpr std::uint8_t max_attempts = 3U;

constexpr std::uint8_t automatic_uid_mode = 0x02U;
constexpr std::uint8_t active_upload = 0x00U;
constexpr std::uint8_t read_once = 0x00U;

} // namespace

reader_core::reader_core(std::uint8_t address) noexcept : address_(address)
{
}

void reader_core::start(std::uint32_t now) noexcept
{
    parser_ = {};
    state_ = {};
    state_.link = link_state::initializing;
    phase_ = phase::settling;
    next_action_ = now + startup_settle_ticks;
    attempts_ = 0U;
}

std::optional<outbound_command> reader_core::poll(std::uint32_t now) noexcept
{
    if ((phase_ == phase::b1_wait || phase_ == phase::b8_wait ||
         phase_ == phase::health_wait) &&
        due(now, deadline_))
    {
        ++state_.timeout_errors;
        fail_pending(now);
    }

    switch (phase_)
    {
    case phase::settling:
        if (due(now, next_action_))
        {
            state_.link = link_state::verifying;
            attempts_ = 0U;
            phase_ = phase::b1_due;
        }
        break;
    case phase::ready:
        if (due(now, next_action_))
        {
            attempts_ = 0U;
            return send(protocol::command::b1, phase::health_wait, now);
        }
        break;
    case phase::recovery_due:
        if (due(now, next_action_))
        {
            state_.link = link_state::verifying;
            attempts_ = 0U;
            phase_ = phase::b1_due;
        }
        break;
    default:
        break;
    }

    if (phase_ == phase::b1_due && due(now, next_action_))
    {
        return send(protocol::command::b1, phase::b1_wait, now);
    }
    if (phase_ == phase::b8_due && due(now, next_action_))
    {
        return send(protocol::command::b8, phase::b8_wait, now);
    }
    if (phase_ == phase::health_due && due(now, next_action_))
    {
        return send(protocol::command::b1, phase::health_wait, now);
    }
    return std::nullopt;
}

void reader_core::feed(const std::uint8_t* bytes, std::size_t size,
                       std::uint32_t now) noexcept
{
    if (size != 0U)
    {
        state_.last_rx_tick = now;
    }

    for (std::size_t i = 0U; i < size; ++i)
    {
        parser_.push(bytes[i]);
        for (;;)
        {
            protocol::frame input{};
            const auto result = parser_.next(input);
            if (result == protocol::parse_result::none)
            {
                break;
            }
            if (result == protocol::parse_result::checksum_error)
            {
                ++state_.checksum_errors;
                continue;
            }
            if (result == protocol::parse_result::frame_error)
            {
                ++state_.frame_errors;
                continue;
            }
            handle_frame(input, now);
        }
    }
}

void reader_core::record_overflow() noexcept
{
    ++state_.overflow_errors;
}

void reader_core::record_io_error() noexcept
{
    state_.link = link_state::io_error;
    phase_ = phase::stopped;
}

state reader_core::snapshot() const noexcept
{
    return state_;
}

std::optional<outbound_command>
reader_core::send(protocol::command kind, phase waiting,
                  std::uint32_t now) noexcept
{
    pending_ = kind;
    pending_health_ = waiting == phase::health_wait;
    ++attempts_;
    deadline_ = now + command_timeout_ticks;
    phase_ = waiting;
    return outbound_command{kind, protocol::make_query(kind, address_)};
}

void reader_core::handle_frame(const protocol::frame& input,
                               std::uint32_t now) noexcept
{
    protocol::uid_report uid{};
    if (protocol::decode_uid(input, address_, uid))
    {
        if (state_.link == link_state::ready)
        {
            state_.card_type = uid.card_type;
            state_.last_uid = uid.uid;
            ++state_.event_count;
            state_.last_uid_tick = now;
        }
        return;
    }

    if ((phase_ == phase::b1_wait || phase_ == phase::health_wait) &&
        input.size >= 3U &&
        input.bytes[2] == static_cast<std::uint8_t>(protocol::command::b1))
    {
        handle_b1(input, now);
        return;
    }
    if (phase_ == phase::b8_wait && input.size >= 3U &&
        input.bytes[2] == static_cast<std::uint8_t>(protocol::command::b8))
    {
        handle_b8(input, now);
        return;
    }
    ++state_.frame_errors;
}

void reader_core::handle_b1(const protocol::frame& input,
                            std::uint32_t now) noexcept
{
    protocol::work_mode mode{};
    if (!protocol::decode_b1(input, address_, mode))
    {
        ++state_.frame_errors;
        fail_pending(now);
        return;
    }
    if (mode.mode != automatic_uid_mode || mode.upload != active_upload)
    {
        state_.link = link_state::config_mismatch;
        phase_ = phase::stopped;
        return;
    }

    if (phase_ == phase::health_wait)
    {
        enter_ready(now);
        return;
    }

    attempts_ = 0U;
    phase_ = phase::b8_due;
    next_action_ = now + command_gap_ticks;
}

void reader_core::handle_b8(const protocol::frame& input,
                            std::uint32_t now) noexcept
{
    protocol::read_behavior behavior{};
    if (!protocol::decode_b8(input, address_, behavior))
    {
        ++state_.frame_errors;
        fail_pending(now);
        return;
    }
    if (behavior.mode != read_once)
    {
        state_.link = link_state::config_mismatch;
        phase_ = phase::stopped;
        return;
    }
    enter_ready(now);
}

void reader_core::fail_pending(std::uint32_t now) noexcept
{
    if (attempts_ < max_attempts)
    {
        next_action_ = now + command_gap_ticks;
        phase_ = pending_ == protocol::command::b1
                     ? (pending_health_ ? phase::health_due : phase::b1_due)
                     : phase::b8_due;
        return;
    }

    state_.link = link_state::timeout;
    phase_ = phase::recovery_due;
    next_action_ = now + recovery_period_ticks;
}

void reader_core::enter_ready(std::uint32_t now) noexcept
{
    state_.link = link_state::ready;
    phase_ = phase::ready;
    attempts_ = 0U;
    next_action_ = now + health_period_ticks;
}

bool reader_core::due(std::uint32_t now, std::uint32_t deadline) noexcept
{
    return static_cast<std::int32_t>(now - deadline) >= 0;
}

} // namespace vehicle::rfid
