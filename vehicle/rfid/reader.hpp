#pragma once

#include <array>
#include <cstdint>

namespace vehicle::rfid
{

enum class link_state : std::uint8_t
{
    disabled,
    initializing,
    verifying,
    ready,
    config_mismatch,
    timeout,
    io_error,
};

struct state
{
    link_state link{link_state::disabled};
    std::array<std::uint8_t, 2U> card_type{};
    std::array<std::uint8_t, 4U> last_uid{};
    std::uint32_t event_count{};
    std::uint32_t last_uid_tick{};
    std::uint32_t last_rx_tick{};
    std::uint32_t checksum_errors{};
    std::uint32_t frame_errors{};
    std::uint32_t overflow_errors{};
    std::uint32_t timeout_errors{};
};

bool init() noexcept;
state snapshot() noexcept;

} // namespace vehicle::rfid
