#include "referee_rx_ring.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

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
    referee::rx_ring<8U> ring;
    constexpr std::array<std::uint8_t, 8U> first{
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U};

    require(ring.push(first.data(), first.size()) == 7U);
    require(ring.drop_count() == 1U);

    std::uint8_t value = 0U;
    for (std::uint8_t expected = 0U; expected < 4U; ++expected)
    {
        require(ring.pop(value));
        require(value == expected);
    }

    constexpr std::array<std::uint8_t, 4U> wrapped{
        8U, 9U, 10U, 11U};
    require(ring.push(wrapped.data(), wrapped.size()) == 4U);
    require(ring.drop_count() == 1U);

    constexpr std::array<std::uint8_t, 7U> expected_after_wrap{
        4U, 5U, 6U, 8U, 9U, 10U, 11U};
    for (const std::uint8_t expected : expected_after_wrap)
    {
        require(ring.pop(value));
        require(value == expected);
    }
    require(!ring.pop(value));

    require(ring.push(nullptr, 1U) == 0U);
    require(ring.drop_count() == 2U);
    require(ring.push(first.data(), 0U) == 0U);
    require(ring.drop_count() == 2U);
    return 0;
}
