#include "bsp_flash.hpp"

#include <array>
#include <cstdlib>

namespace host_test::fake_flash
{
void reset() noexcept;
std::size_t erases() noexcept;
std::size_t programs() noexcept;
} // namespace host_test::fake_flash

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

} // namespace

int main()
{
    host_test::fake_flash::reset();
    const bsp::flash::geometry geometry = bsp::flash::layout();
    require(geometry.begin == 0x08000000U);
    require(geometry.end == 0x08100000U);
    require(geometry.program_alignment == 4U);

    std::array<std::uint8_t, 8U> data{};
    require(bsp::flash::program(
                geometry.begin, nullptr, data.size()) ==
            types::status::invalid_arg);
    require(bsp::flash::program(
                geometry.begin + 1U, data.data(), data.size()) ==
            types::status::invalid_arg);
    require(bsp::flash::program(
                geometry.end - 4U, data.data(), data.size()) ==
            types::status::invalid_arg);
    require(bsp::flash::erase_block(geometry.end) ==
            types::status::invalid_arg);

    require(bsp::flash::erase_block(geometry.begin) ==
            types::status::ok);
    require(bsp::flash::program(
                geometry.begin, data.data(), data.size()) ==
            types::status::ok);
    require(host_test::fake_flash::erases() == 1U);
    require(host_test::fake_flash::programs() == 1U);
    return EXIT_SUCCESS;
}
