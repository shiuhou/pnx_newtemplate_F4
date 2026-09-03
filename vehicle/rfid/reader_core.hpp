#pragma once

#include "vehicle/rfid/protocol.hpp"
#include "vehicle/rfid/reader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace vehicle::rfid
{

struct outbound_command
{
    protocol::command kind{};
    std::array<std::uint8_t, 8U> bytes{};
};

class reader_core
{
public:
    explicit reader_core(std::uint8_t address) noexcept;

    void start(std::uint32_t now) noexcept;
    std::optional<outbound_command> poll(std::uint32_t now) noexcept;
    void feed(const std::uint8_t* bytes, std::size_t size,
              std::uint32_t now) noexcept;
    void record_overflow() noexcept;
    void record_io_error() noexcept;
    state snapshot() const noexcept;

private:
    enum class phase : std::uint8_t
    {
        disabled,
        settling,
        b1_due,
        b1_wait,
        b8_due,
        b8_wait,
        ready,
        health_due,
        health_wait,
        recovery_due,
        stopped,
    };

    std::optional<outbound_command> send(protocol::command kind,
                                         phase waiting,
                                         std::uint32_t now) noexcept;
    void handle_frame(const protocol::frame& input,
                      std::uint32_t now) noexcept;
    void handle_b1(const protocol::frame& input,
                   std::uint32_t now) noexcept;
    void handle_b8(const protocol::frame& input,
                   std::uint32_t now) noexcept;
    void fail_pending(std::uint32_t now) noexcept;
    void enter_ready(std::uint32_t now) noexcept;

    static bool due(std::uint32_t now, std::uint32_t deadline) noexcept;

    std::uint8_t address_{};
    protocol::parser parser_{};
    state state_{};
    phase phase_{phase::disabled};
    protocol::command pending_{protocol::command::b1};
    std::uint32_t deadline_{};
    std::uint32_t next_action_{};
    std::uint8_t attempts_{};
    bool pending_health_{};
};

} // namespace vehicle::rfid
