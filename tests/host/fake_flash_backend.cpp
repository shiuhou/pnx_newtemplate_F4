#include "bsp_flash.hpp"

#include <cstddef>
#include <cstdint>

namespace
{

std::size_t erase_calls = 0U;
std::size_t program_calls = 0U;

} // namespace

namespace host_test::fake_flash
{

void reset() noexcept
{
    erase_calls = 0U;
    program_calls = 0U;
}

std::size_t erases() noexcept
{
    return erase_calls;
}

std::size_t programs() noexcept
{
    return program_calls;
}

} // namespace host_test::fake_flash

namespace bsp::flash::detail
{

geometry backend_geometry() noexcept
{
    return {0x08000000U, 0x08100000U, 4U};
}

types::status backend_erase_block(std::uint32_t) noexcept
{
    ++erase_calls;
    return types::status::ok;
}

types::status backend_program(
    std::uint32_t, const std::uint8_t*, std::size_t) noexcept
{
    ++program_calls;
    return types::status::ok;
}

} // namespace bsp::flash::detail
