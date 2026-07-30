#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bsp::usart
{

using port = std::size_t;
inline constexpr port port_count = 4U;

enum class handle_id
{
    fake0,
    fake1,
    fake2,
    fake3,
};

struct port_config
{
    bool enabled = false;
    handle_id handle = handle_id::fake0;
    bool has_rx_dma = false;
    bool has_tx_dma = false;
};

inline constexpr std::array<port_config, port_count> configs{{
    {true, handle_id::fake0, true, true},
    {true, handle_id::fake1, true, true},
    {true, handle_id::fake2, true, true},
    {true, handle_id::fake3, true, true},
}};

} // namespace bsp::usart

namespace bsp::can
{

enum class bus_type : std::uint8_t
{
    classic = 0,
    fd = 1,
};

enum class id_type : std::uint8_t
{
    standard = 0,
    extended = 1,
};

enum class handle_id : std::uint8_t
{
    none = 0,
    fake0,
    fake1,
};

enum class bus : std::uint8_t
{
    can1 = 0,
    can2,
    fdcan1 = can1,
    fdcan2 = can2,
    none,
};

struct bus_config
{
    bool enabled = false;
    handle_id handle = handle_id::none;
    bus_type type = bus_type::classic;
    id_type filter_id_type = id_type::standard;
};

inline constexpr std::size_t bus_count = 2U;
inline constexpr std::size_t max_rx_callbacks = 4U;
inline constexpr std::uint32_t tx_delay_comp_tdc = 0U;
inline constexpr std::uint32_t tx_delay_comp_filter = 0U;

inline constexpr std::array<bus_config, bus_count> configs{{
    {true, handle_id::fake0, bus_type::classic, id_type::standard},
    {true, handle_id::fake1, bus_type::classic, id_type::standard},
}};

} // namespace bsp::can

namespace params::usb
{

inline constexpr std::uint32_t read_thread_priority = 5U;
inline constexpr std::uint32_t write_thread_priority = 5U;
inline constexpr std::uint32_t period_ticks = 2U;

} // namespace params::usb
