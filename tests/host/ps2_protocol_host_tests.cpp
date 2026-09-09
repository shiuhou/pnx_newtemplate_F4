#include "ps2.hpp"

#include <array>
#include <cmath>
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

bool near(float lhs, float rhs) noexcept
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

struct fake_transport
{
    types::status result = types::status::ok;
    remoter::ps2_raw_frame response{};
    std::array<std::uint8_t, remoter::ps2_protocol::frame_size> query{};

    types::status transfer(const std::uint8_t* tx, std::uint8_t* rx,
                           std::size_t size) noexcept
    {
        require(tx != nullptr && rx != nullptr && size == query.size());
        for (std::size_t index = 0U; index < size; ++index)
        {
            query[index] = tx[index];
            rx[index] = response.bytes[index];
        }
        return result;
    }
};

remoter::ps2_raw_frame valid_frame(std::uint8_t id = 0x73U) noexcept
{
    remoter::ps2_raw_frame frame{};
    frame.bytes = {0xFFU, id, 0x5AU, 0xFFU, 0xFFU,
                   0x7FU, 0x80U, 0x7FU, 0x80U};
    return frame;
}

void test_query_decode_and_block_0x41()
{
    fake_transport transport{};
    transport.response = valid_frame();
    transport.response.bytes[3] &= static_cast<std::uint8_t>(~(1U << 4U));
    transport.response.bytes[5] = 0xFFU;
    transport.response.bytes[6] = 0x00U;

    remoter::ps2_controller_state decoded{};
    require(remoter::ps2_protocol::poll(transport, decoded) ==
            types::status::ok);
    require(transport.query == std::array<std::uint8_t, 9U>{
        0x01U, 0x42U, 0U, 0U, 0U, 0U, 0U, 0U, 0U});
    require(decoded.up);
    require(near(decoded.rx, 1.0F));
    require(near(decoded.ry, 1.0F));

    auto digital = valid_frame(0x41U);
    require(remoter::ps2_protocol::decode(digital, decoded) ==
            types::status::error);
}

void test_invalid_frame_fails_closed()
{
    remoter::ps2_state state{};
    remoter::ps2_protocol::update_state(
        state, types::status::error, {}, valid_frame(0x41U), 10U);
    require(state.data.offline);
    require(state.data.ps2_buttons == 0U);
    require(near(state.data.left_x, 0.0F));
    require(state.error_count == 1U);
}

} // namespace

int main()
{
    test_query_decode_and_block_0x41();
    test_invalid_frame_fails_closed();
    return EXIT_SUCCESS;
}
