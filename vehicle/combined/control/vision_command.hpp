#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace vehicle::combined
{

inline constexpr std::size_t vision_frame_size = 24U;
inline constexpr std::size_t vision_queue_capacity = 128U;
inline constexpr float vision_max_axis_speed_mps = 0.8F;
inline constexpr std::uint32_t vision_timeout_ticks = 200U;

std::uint32_t vision_checksum(const std::uint8_t* data,
                              std::size_t length) noexcept;
bool sequence_is_newer(std::uint32_t current,
                       std::uint32_t previous) noexcept;

struct vision_command_snapshot {
    std::uint32_t seq{};
    float vx_mps{};
    float vy_mps{};
    bool valid{};
    bool seen{};
    std::uint32_t received_tick{};
    std::uint32_t generation{};
    std::uint32_t accepted_count{};
    std::uint32_t rejected_count{};
    std::uint32_t checksum_error_count{};
    std::uint32_t overflow_count{};
};

struct vision_auto_gate_input {
    bool ps2_online{};
    bool globally_unlocked{};
    bool auto_mode{};
    bool l1_held{};
    std::uint32_t entry_generation{};
    vision_command_snapshot command{};
    std::uint32_t now_tick{};
    bool chassis_healthy{};
};

bool vision_auto_motion_allowed(
    const vision_auto_gate_input& input) noexcept;

// USART ISR 只调用 push_from_isr() 做一次持久复制；process() 由 5 ms
// control loop 调用，避免在中断内执行协议与控制策略。
class vision_command_receiver {
public:
    bool push_from_isr(const std::uint8_t* data,
                       std::size_t length) noexcept;
    void process(std::uint32_t now_tick) noexcept;
    vision_command_snapshot snapshot() const noexcept;
    void reset() noexcept;

private:
    void consume(std::uint8_t byte, std::uint32_t now_tick) noexcept;
    void discard_parser_prefix(std::size_t count) noexcept;
    void invalidate_after_overflow(std::uint32_t overflow_count) noexcept;

    std::array<std::uint8_t, vision_queue_capacity> queue_{};
    std::atomic<std::uint32_t> write_index_{0U};
    std::atomic<std::uint32_t> read_index_{0U};
    std::atomic<std::uint32_t> overflow_count_{0U};
    std::uint32_t observed_overflow_count_{};

    std::array<std::uint8_t, vision_frame_size> parser_{};
    std::size_t parser_size_{};
    bool sequence_seen_{};
    vision_command_snapshot snapshot_{};
};

} // namespace vehicle::combined
