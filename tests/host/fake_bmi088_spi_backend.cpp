#include "bsp_spi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace
{

bool ready = false;
int active_select = -1;
std::uint8_t address = 0U;
std::size_t writes = 0U;

constexpr std::array<std::uint8_t, 6U> accel_raw{
    0x34U, 0x12U, 0xFEU, 0xFFU, 0x02U, 0x01U};
constexpr std::array<std::uint8_t, 6U> gyro_raw{
    0x78U, 0x56U, 0xFDU, 0xFFU, 0x04U, 0x03U};

} // namespace

namespace host_test::fake_bmi088
{

void reset() noexcept
{
    ready = false;
    active_select = -1;
    address = 0U;
    writes = 0U;
}

std::size_t write_count() noexcept
{
    return writes;
}

} // namespace host_test::fake_bmi088

namespace bsp::spi::detail
{

bool backend_bus_enabled(bus selected) noexcept
{
    return selected == bus{0U};
}

bool backend_select_enabled(chip_select selected) noexcept
{
    return selected.value < 2U;
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
    bus selected, const std::uint8_t* data, std::size_t len,
    std::uint32_t) noexcept
{
    if (!backend_bus_enabled(selected) || !ready ||
        active_select < 0 || data == nullptr || len == 0U)
    {
        return types::status::error;
    }
    address = static_cast<std::uint8_t>(data[0] & 0x7FU);
    if ((data[0] & 0x80U) == 0U)
    {
        ++writes;
    }
    return types::status::ok;
}

types::status backend_receive(
    bus selected, std::uint8_t* data, std::size_t len,
    std::uint32_t) noexcept
{
    if (!backend_bus_enabled(selected) || !ready ||
        active_select < 0 || data == nullptr || len == 0U)
    {
        return types::status::error;
    }

    for (std::size_t index = 0U; index < len; ++index)
    {
        data[index] = 0U;
    }
    if (address == 0x00U)
    {
        if (active_select == 0)
        {
            data[len - 1U] = 0x1EU;
        }
        else
        {
            data[0] = 0x0FU;
        }
    }
    else if (active_select == 0 && address == 0x12U)
    {
        for (std::size_t index = 0U;
             index < accel_raw.size() && index + 1U < len; ++index)
        {
            data[index + 1U] = accel_raw[index];
        }
    }
    else if (active_select == 0 && address == 0x22U &&
             len >= 3U)
    {
        // 25 C: raw 11-bit value 16 -> 16 * 0.125 + 23.
        data[1U] = 0x02U;
        data[2U] = 0x00U;
    }
    else if (active_select == 1 && address == 0x02U)
    {
        for (std::size_t index = 0U;
             index < gyro_raw.size() && index < len; ++index)
        {
            data[index] = gyro_raw[index];
        }
    }
    return types::status::ok;
}

types::status backend_set_select(
    chip_select selected, bool active) noexcept
{
    if (!backend_select_enabled(selected))
    {
        return types::status::not_configured;
    }
    active_select = active ? static_cast<int>(selected.value) : -1;
    return types::status::ok;
}

} // namespace bsp::spi::detail
