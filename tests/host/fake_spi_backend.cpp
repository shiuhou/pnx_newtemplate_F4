#include "fake_spi_backend.hpp"

#include "bsp_spi.hpp"

#include <array>

namespace
{

bool ready = false;
std::array<bool, 2U> selected_lines{};
std::size_t writes = 0U;
std::size_t reads = 0U;

} // namespace

namespace host_test::fake_spi
{

void reset() noexcept
{
    ready = false;
    selected_lines = {};
    writes = 0U;
    reads = 0U;
}

bool initialized() noexcept
{
    return ready;
}

bool selected(std::uint8_t line) noexcept
{
    return line < selected_lines.size() && selected_lines[line];
}

std::size_t transmit_count() noexcept
{
    return writes;
}

std::size_t receive_count() noexcept
{
    return reads;
}

} // namespace host_test::fake_spi

namespace bsp::spi::detail
{

bool backend_bus_enabled(bus selected) noexcept
{
    return selected == bus{0U};
}

bool backend_select_enabled(chip_select selected) noexcept
{
    return selected.value < selected_lines.size();
}

types::status backend_init(bus selected) noexcept
{
    if (!backend_bus_enabled(selected))
    {
        return types::status::not_configured;
    }
    ready = true;
    return types::status::ok;
}

types::status backend_wait_ready(bus selected, std::uint32_t) noexcept
{
    return backend_bus_enabled(selected) && ready
               ? types::status::ok
               : types::status::not_configured;
}

types::status backend_transmit(
    bus selected, const std::uint8_t*, std::size_t,
    std::uint32_t) noexcept
{
    if (!backend_bus_enabled(selected) || !ready)
    {
        return types::status::not_configured;
    }
    ++writes;
    return types::status::ok;
}

types::status backend_receive(
    bus selected, std::uint8_t* data, std::size_t len,
    std::uint32_t) noexcept
{
    if (!backend_bus_enabled(selected) || !ready)
    {
        return types::status::not_configured;
    }
    for (std::size_t index = 0U; index < len; ++index)
    {
        data[index] = static_cast<std::uint8_t>(index + 1U);
    }
    ++reads;
    return types::status::ok;
}

types::status backend_set_select(
    chip_select selected, bool active) noexcept
{
    if (!backend_select_enabled(selected))
    {
        return types::status::not_configured;
    }
    selected_lines[selected.value] = active;
    return types::status::ok;
}

} // namespace bsp::spi::detail
