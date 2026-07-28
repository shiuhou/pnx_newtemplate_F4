#include "dr16_protocol.hpp"

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

std::array<std::uint8_t, remoter::dr16_frame_size>
make_frame(
    std::uint16_t ch0, std::uint16_t ch1,
    std::uint16_t ch2, std::uint16_t ch3,
    std::uint8_t s1, std::uint8_t s2) noexcept
{
    std::array<std::uint8_t, remoter::dr16_frame_size> bytes{};
    bytes[0] = static_cast<std::uint8_t>(ch0);
    bytes[1] = static_cast<std::uint8_t>(
        (ch0 >> 8U) | (ch1 << 3U));
    bytes[2] = static_cast<std::uint8_t>(
        (ch1 >> 5U) | (ch2 << 6U));
    bytes[3] = static_cast<std::uint8_t>(ch2 >> 2U);
    bytes[4] = static_cast<std::uint8_t>(
        (ch2 >> 10U) | (ch3 << 1U));
    bytes[5] = static_cast<std::uint8_t>(
        (ch3 >> 7U) | (s1 << 4U) | (s2 << 6U));
    return bytes;
}

} // namespace

int main()
{
    auto frame = make_frame(1024U, 1024U, 364U, 1684U, 1U, 2U);
    frame[6] = 0x34U;
    frame[7] = 0x12U;
    frame[8] = 0xFEU;
    frame[9] = 0xFFU;
    frame[12] = 1U;
    frame[14] = 0x01U;
    frame[15] = 0x80U;

    remoter::state decoded{};
    require(remoter::decode_dr16_frame(
        frame.data(), frame.size(), decoded));
    require(!decoded.offline);
    require(decoded.active_source == remoter::source::dr16);
    require(decoded.right_x == 0.0F);
    require(decoded.right_y == 0.0F);
    require(decoded.left_x == -1.0F);
    require(decoded.left_y == 1.0F);
    require(decoded.right_sw == remoter::sw_state::up);
    require(decoded.left_sw == remoter::sw_state::low);
    require(decoded.mouse_x == 4660.0F);
    require(decoded.mouse_y == -2.0F);
    require(decoded.mouse_left);
    require(decoded.key.W == 1U);
    require(decoded.key.B == 1U);

    frame[5] &= 0x0FU;
    require(!remoter::decode_dr16_frame(
        frame.data(), frame.size(), decoded));
    require(!remoter::decode_dr16_frame(
        frame.data(), frame.size() - 1U, decoded));
    return EXIT_SUCCESS;
}
