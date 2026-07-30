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

namespace bsp::flash
{

geometry layout() noexcept
{
    return {0x08000000U, 0x08100000U, 4U};
}

types::status erase_block(std::uint32_t address) noexcept
{
    const geometry current = layout();
    if (current.begin >= current.end ||
        address < current.begin || address >= current.end)
    {
        return types::status::invalid_arg;
    }
    ++erase_calls;
    return types::status::ok;
}

types::status program(
    std::uint32_t address, const void* data, std::size_t len) noexcept
{
    const geometry current = layout();
    if (data == nullptr || len == 0U ||
        current.begin >= current.end ||
        current.program_alignment == 0U ||
        address < current.begin || address >= current.end ||
        (address % current.program_alignment) != 0U ||
        (len % current.program_alignment) != 0U ||
        len > static_cast<std::size_t>(current.end - address))
    {
        return types::status::invalid_arg;
    }
    ++program_calls;
    return types::status::ok;
}

} // namespace bsp::flash
