#include "vehicle/combined/control/vision_command.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace vehicle::combined
{
namespace
{

constexpr std::uint32_t magic = 0x31435641U;
constexpr std::uint32_t checksum_seed = 0xA5A51234U;
constexpr std::size_t payload_size = 20U;

std::uint32_t read_u32(const std::uint8_t* data) noexcept
{
    return static_cast<std::uint32_t>(data[0U]) |
           (static_cast<std::uint32_t>(data[1U]) << 8U) |
           (static_cast<std::uint32_t>(data[2U]) << 16U) |
           (static_cast<std::uint32_t>(data[3U]) << 24U);
}

float read_float(const std::uint8_t* data) noexcept
{
    const std::uint32_t bits = read_u32(data);
    float value{};
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool starts_with_magic(
    const std::array<std::uint8_t, vision_frame_size>& data,
    std::size_t size) noexcept
{
    return size >= 4U && read_u32(data.data()) == magic;
}

} // namespace

std::uint32_t vision_checksum(const std::uint8_t* data,
                              std::size_t length) noexcept
{
    std::uint32_t checksum = checksum_seed;
    if (data == nullptr)
    {
        return checksum;
    }
    for (std::size_t index = 0U; index < length; ++index)
    {
        checksum = (checksum << 5U) ^ (checksum >> 2U) ^ data[index];
    }
    return checksum;
}

bool sequence_is_newer(std::uint32_t current,
                       std::uint32_t previous) noexcept
{
    return static_cast<std::int32_t>(current - previous) > 0;
}

bool vision_auto_motion_allowed(
    const vision_auto_gate_input& input) noexcept
{
    return input.ps2_online && input.globally_unlocked &&
           input.auto_mode && input.l1_held &&
           input.command.seen && input.command.valid &&
           input.command.generation != input.entry_generation &&
           (input.now_tick - input.command.received_tick) <=
               vision_timeout_ticks &&
           input.chassis_healthy;
}

bool vision_command_receiver::push_from_isr(
    const std::uint8_t* data, std::size_t length) noexcept
{
    if (length == 0U)
    {
        return true;
    }
    if (data == nullptr)
    {
        return false;
    }

    const std::uint32_t write =
        write_index_.load(std::memory_order_relaxed);
    const std::uint32_t read =
        read_index_.load(std::memory_order_acquire);
    const std::uint32_t queued = write - read;
    if (queued > vision_queue_capacity ||
        length > vision_queue_capacity -
                     static_cast<std::size_t>(queued))
    {
        // USART6 的 DMA callback buffer 会马上被 BSP 重用。队列装不下时
        // 整段丢弃，并让 control loop 在下一周期统一作废命令。
        read_index_.store(write, std::memory_order_release);
        overflow_count_.fetch_add(1U, std::memory_order_release);
        return false;
    }

    for (std::size_t index = 0U; index < length; ++index)
    {
        queue_[(write + static_cast<std::uint32_t>(index)) %
               vision_queue_capacity] = data[index];
    }
    write_index_.store(
        write + static_cast<std::uint32_t>(length),
        std::memory_order_release);
    return true;
}

void vision_command_receiver::process(std::uint32_t now_tick) noexcept
{
    const std::uint32_t overflow_before =
        overflow_count_.load(std::memory_order_acquire);
    if (overflow_before != observed_overflow_count_)
    {
        invalidate_after_overflow(overflow_before);
        return;
    }

    for (;;)
    {
        const std::uint32_t read =
            read_index_.load(std::memory_order_relaxed);
        const std::uint32_t write =
            write_index_.load(std::memory_order_acquire);
        if (read == write)
        {
            break;
        }
        const std::uint8_t byte = queue_[read % vision_queue_capacity];
        read_index_.store(read + 1U, std::memory_order_release);
        consume(byte, now_tick);
    }

    const std::uint32_t overflow_after =
        overflow_count_.load(std::memory_order_acquire);
    if (overflow_after != observed_overflow_count_)
    {
        invalidate_after_overflow(overflow_after);
        return;
    }
    snapshot_.overflow_count = overflow_after;
}

vision_command_snapshot vision_command_receiver::snapshot() const noexcept
{
    auto copy = snapshot_;
    copy.overflow_count =
        overflow_count_.load(std::memory_order_acquire);
    return copy;
}

void vision_command_receiver::reset() noexcept
{
    write_index_.store(0U, std::memory_order_relaxed);
    read_index_.store(0U, std::memory_order_relaxed);
    overflow_count_.store(0U, std::memory_order_relaxed);
    observed_overflow_count_ = 0U;
    parser_ = {};
    parser_size_ = 0U;
    sequence_seen_ = false;
    snapshot_ = {};
}

void vision_command_receiver::consume(std::uint8_t byte,
                                      std::uint32_t now_tick) noexcept
{
    parser_[parser_size_++] = byte;
    while (parser_size_ >= 4U &&
           !starts_with_magic(parser_, parser_size_))
    {
        discard_parser_prefix(1U);
    }
    if (parser_size_ != vision_frame_size)
    {
        return;
    }

    const std::uint32_t expected_checksum =
        read_u32(parser_.data() + payload_size);
    if (vision_checksum(parser_.data(), payload_size) != expected_checksum)
    {
        ++snapshot_.checksum_error_count;
        ++snapshot_.rejected_count;
        discard_parser_prefix(1U);
        return;
    }

    const std::uint8_t valid = parser_[16U];
    const float vx_mps = read_float(parser_.data() + 8U);
    const float vy_mps = read_float(parser_.data() + 12U);
    const bool fields_valid =
        valid <= 1U && parser_[17U] == 0U && parser_[18U] == 0U &&
        parser_[19U] == 0U && std::isfinite(vx_mps) &&
        std::isfinite(vy_mps);
    if (!fields_valid)
    {
        ++snapshot_.rejected_count;
        discard_parser_prefix(1U);
        return;
    }

    const std::uint32_t seq = read_u32(parser_.data() + 4U);
    parser_size_ = 0U;
    if (sequence_seen_ && !sequence_is_newer(seq, snapshot_.seq))
    {
        ++snapshot_.rejected_count;
        return;
    }

    sequence_seen_ = true;
    snapshot_.seq = seq;
    snapshot_.vx_mps = std::clamp(
        vx_mps, -vision_max_axis_speed_mps,
        vision_max_axis_speed_mps);
    snapshot_.vy_mps = std::clamp(
        vy_mps, -vision_max_axis_speed_mps,
        vision_max_axis_speed_mps);
    snapshot_.valid = valid == 1U;
    snapshot_.seen = true;
    snapshot_.received_tick = now_tick;
    ++snapshot_.generation;
    ++snapshot_.accepted_count;
}

void vision_command_receiver::discard_parser_prefix(
    std::size_t count) noexcept
{
    if (count >= parser_size_)
    {
        parser_size_ = 0U;
        return;
    }
    std::move(parser_.begin() + static_cast<std::ptrdiff_t>(count),
              parser_.begin() + static_cast<std::ptrdiff_t>(parser_size_),
              parser_.begin());
    parser_size_ -= count;
}

void vision_command_receiver::invalidate_after_overflow(
    std::uint32_t overflow_count) noexcept
{
    parser_size_ = 0U;
    snapshot_.seen = false;
    snapshot_.valid = false;
    snapshot_.overflow_count = overflow_count;
    observed_overflow_count_ = overflow_count;
    read_index_.store(
        write_index_.load(std::memory_order_acquire),
        std::memory_order_release);
}

} // namespace vehicle::combined
