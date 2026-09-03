#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vehicle::rfid::protocol
{

inline constexpr std::size_t min_frame_size = 8U;
inline constexpr std::size_t max_frame_size = 32U;

enum class command : std::uint8_t
{
    b1 = 0xB1U,
    b8 = 0xB8U,
};

struct frame
{
    std::array<std::uint8_t, max_frame_size> bytes{};
    std::uint8_t size{};
};

enum class parse_result : std::uint8_t
{
    none,
    frame,
    checksum_error,
    frame_error,
};

class parser
{
public:
    void push(std::uint8_t byte) noexcept;
    parse_result next(frame& output) noexcept;

private:
    void discard(std::size_t count) noexcept;

    std::array<std::uint8_t, max_frame_size> buffer_{};
    std::size_t size_{};
    bool resynchronizing_{};
};

struct uid_report
{
    std::array<std::uint8_t, 2U> card_type{};
    std::array<std::uint8_t, 4U> uid{};
};

struct work_mode
{
    std::uint8_t mode{};
    std::uint8_t block{};
    std::uint8_t upload{};
};

struct read_behavior
{
    std::uint8_t mode{};
};

std::uint8_t checksum(const std::uint8_t* bytes,
                      std::size_t size) noexcept;
std::array<std::uint8_t, 8U> make_query(command kind,
                                        std::uint8_t address) noexcept;
bool decode_uid(const frame& input, std::uint8_t address,
                uid_report& output) noexcept;
bool decode_b1(const frame& input, std::uint8_t address,
               work_mode& output) noexcept;
bool decode_b8(const frame& input, std::uint8_t address,
               read_behavior& output) noexcept;

} // namespace vehicle::rfid::protocol
