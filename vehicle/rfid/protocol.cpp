#include "vehicle/rfid/protocol.hpp"

#include <algorithm>

namespace vehicle::rfid::protocol
{
namespace
{

constexpr std::uint8_t command_packet = 0x02U;
constexpr std::uint8_t automatic_packet = 0x04U;
constexpr std::uint8_t automatic_uid = 0x02U;
constexpr std::uint8_t success = 0x00U;

bool plausible_header(std::uint8_t byte) noexcept
{
    return byte == command_packet || byte == automatic_packet;
}

bool contains_complete_frame(const std::uint8_t* bytes,
                             std::size_t size) noexcept
{
    for (std::size_t offset = 1U; offset + 1U < size; ++offset)
    {
        if (!plausible_header(bytes[offset]))
        {
            continue;
        }
        const std::size_t candidate_size = bytes[offset + 1U];
        if (candidate_size < min_frame_size ||
            candidate_size > max_frame_size ||
            candidate_size > size - offset)
        {
            continue;
        }
        if (checksum(bytes + offset, candidate_size - 1U) ==
            bytes[offset + candidate_size - 1U])
        {
            return true;
        }
    }
    return false;
}

bool is_response(const frame& input, std::size_t expected_size,
                 command expected, std::uint8_t address) noexcept
{
    return input.size == expected_size &&
           input.bytes[0] == command_packet &&
           input.bytes[1] == expected_size &&
           input.bytes[2] == static_cast<std::uint8_t>(expected) &&
           input.bytes[3] == address && input.bytes[4] == success;
}

} // namespace

std::uint8_t checksum(const std::uint8_t* bytes,
                      std::size_t size) noexcept
{
    std::uint8_t value{};
    for (std::size_t i = 0U; i < size; ++i)
    {
        value ^= bytes[i];
    }
    return static_cast<std::uint8_t>(~value);
}

std::array<std::uint8_t, 8U> make_query(command kind,
                                        std::uint8_t address) noexcept
{
    std::array<std::uint8_t, 8U> query{
        command_packet, 0x08U, static_cast<std::uint8_t>(kind), address,
        0x00U,          0x00U, 0x00U,                           0x00U,
    };
    query.back() = checksum(query.data(), query.size() - 1U);
    return query;
}

void parser::push(std::uint8_t byte) noexcept
{
    if (size_ == buffer_.size())
    {
        discard(1U);
        resynchronizing_ = true;
    }
    buffer_[size_++] = byte;
}

parse_result parser::next(frame& output) noexcept
{
    if (size_ == 0U)
    {
        return parse_result::none;
    }

    if (!plausible_header(buffer_[0]))
    {
        discard(1U);
        resynchronizing_ = true;
        return parse_result::frame_error;
    }
    if (size_ < 2U)
    {
        return parse_result::none;
    }

    const std::size_t declared_size = buffer_[1];
    if (declared_size < min_frame_size || declared_size > max_frame_size)
    {
        discard(1U);
        resynchronizing_ = true;
        return parse_result::frame_error;
    }
    if (size_ < declared_size)
    {
        if (resynchronizing_ &&
            contains_complete_frame(buffer_.data(), size_))
        {
            discard(1U);
            return parse_result::frame_error;
        }
        return parse_result::none;
    }
    if (checksum(buffer_.data(), declared_size - 1U) !=
        buffer_[declared_size - 1U])
    {
        discard(1U);
        resynchronizing_ = true;
        return parse_result::checksum_error;
    }

    std::copy_n(buffer_.begin(), declared_size, output.bytes.begin());
    output.size = static_cast<std::uint8_t>(declared_size);
    discard(declared_size);
    resynchronizing_ = false;
    return parse_result::frame;
}

void parser::discard(std::size_t count) noexcept
{
    const std::size_t remaining = size_ - count;
    std::move(buffer_.begin() + static_cast<std::ptrdiff_t>(count),
              buffer_.begin() + static_cast<std::ptrdiff_t>(size_),
              buffer_.begin());
    size_ = remaining;
}

bool decode_uid(const frame& input, std::uint8_t address,
                uid_report& output) noexcept
{
    if (input.size != 12U || input.bytes[0] != automatic_packet ||
        input.bytes[1] != 0x0CU || input.bytes[2] != automatic_uid ||
        input.bytes[3] != address || input.bytes[4] != success)
    {
        return false;
    }

    std::copy_n(input.bytes.begin() + 5, output.card_type.size(),
                output.card_type.begin());
    std::copy_n(input.bytes.begin() + 7, output.uid.size(),
                output.uid.begin());
    return true;
}

bool decode_b1(const frame& input, std::uint8_t address,
               work_mode& output) noexcept
{
    if (!is_response(input, 9U, command::b1, address))
    {
        return false;
    }
    output.mode = input.bytes[5];
    output.block = input.bytes[6];
    output.upload = input.bytes[7];
    return true;
}

bool decode_b8(const frame& input, std::uint8_t address,
               read_behavior& output) noexcept
{
    if (!is_response(input, 8U, command::b8, address))
    {
        return false;
    }
    output.mode = input.bytes[5];
    return true;
}

} // namespace vehicle::rfid::protocol
