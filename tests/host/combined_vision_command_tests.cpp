#include "vehicle/combined/control/vision_command.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{

using vehicle::combined::sequence_is_newer;
using vehicle::combined::vision_command_receiver;
using vehicle::combined::vision_frame_size;

constexpr std::array<std::uint8_t, vision_frame_size> golden_valid{
    0x41, 0x56, 0x43, 0x31, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0xBE,
    0x01, 0x00, 0x00, 0x00, 0x2E, 0xB8, 0x88, 0x88,
};
constexpr std::array<std::uint8_t, vision_frame_size> golden_stop{
    0x41, 0x56, 0x43, 0x31, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xDE, 0xB9, 0xEB, 0x44,
};
constexpr std::array<std::uint8_t, vision_frame_size> golden_wrap{
    0x41, 0x56, 0x43, 0x31, 0x00, 0x00, 0x00, 0x00,
    0xCD, 0xCC, 0x4C, 0xBF, 0xCD, 0xCC, 0x4C, 0x3F,
    0x01, 0x00, 0x00, 0x00, 0xED, 0x6D, 0x71, 0x00,
};

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool near(float actual, float expected) noexcept
{
    return std::fabs(actual - expected) < 0.0001F;
}

void write_u32(std::array<std::uint8_t, vision_frame_size>& frame,
               std::size_t offset, std::uint32_t value) noexcept
{
    for (std::size_t index = 0U; index < 4U; ++index)
    {
        frame[offset + index] = static_cast<std::uint8_t>(
            value >> (index * 8U));
    }
}

void write_float(std::array<std::uint8_t, vision_frame_size>& frame,
                 std::size_t offset, float value) noexcept
{
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(frame, offset, bits);
}

std::array<std::uint8_t, vision_frame_size> frame_of(
    std::uint32_t seq, float vx, float vy, std::uint8_t valid) noexcept
{
    auto frame = golden_valid;
    write_u32(frame, 4U, seq);
    write_float(frame, 8U, vx);
    write_float(frame, 12U, vy);
    frame[16U] = valid;
    frame[17U] = 0U;
    frame[18U] = 0U;
    frame[19U] = 0U;
    write_u32(frame, 20U,
              vehicle::combined::vision_checksum(frame.data(), 20U));
    return frame;
}

void ingest(vision_command_receiver& receiver,
            const std::array<std::uint8_t, vision_frame_size>& frame,
            std::uint32_t tick) noexcept
{
    require(receiver.push_from_isr(frame.data(), frame.size()));
    receiver.process(tick);
}

void test_golden_vectors_and_stop() noexcept
{
    require(vehicle::combined::vision_checksum(golden_valid.data(), 20U) ==
            0x8888B82EU);
    require(vehicle::combined::vision_checksum(golden_stop.data(), 20U) ==
            0x44EBB9DEU);
    require(vehicle::combined::vision_checksum(golden_wrap.data(), 20U) ==
            0x00716DEDU);

    vision_command_receiver receiver{};
    ingest(receiver, golden_valid, 10U);
    auto snapshot = receiver.snapshot();
    require(snapshot.seen && snapshot.valid);
    require(snapshot.seq == 1U && snapshot.generation == 1U);
    require(snapshot.received_tick == 10U);
    require(near(snapshot.vx_mps, 0.5F));
    require(near(snapshot.vy_mps, -0.25F));

    ingest(receiver, golden_stop, 20U);
    snapshot = receiver.snapshot();
    require(snapshot.seen && !snapshot.valid);
    require(snapshot.seq == 2U && snapshot.generation == 2U);
    require(snapshot.received_tick == 20U);
}

void test_stream_split_concatenated_garbage_and_resync() noexcept
{
    vision_command_receiver split{};
    require(split.push_from_isr(golden_valid.data(), 7U));
    split.process(1U);
    require(!split.snapshot().seen);
    require(split.push_from_isr(golden_valid.data() + 7U,
                                golden_valid.size() - 7U));
    split.process(2U);
    require(split.snapshot().seen);

    vision_command_receiver combined{};
    std::array<std::uint8_t, 51U> stream{0x99U, 0x41U, 0x00U};
    std::memcpy(stream.data() + 3U, golden_valid.data(), golden_valid.size());
    std::memcpy(stream.data() + 3U + golden_valid.size(),
                golden_stop.data(), golden_stop.size());
    require(combined.push_from_isr(stream.data(), stream.size()));
    combined.process(30U);
    const auto concatenated = combined.snapshot();
    require(concatenated.seq == 2U);
    require(concatenated.generation == 2U);

    vision_command_receiver resync{};
    auto bad = golden_valid;
    bad[20U] ^= 0x01U;
    std::array<std::uint8_t, vision_frame_size * 2U> bad_then_good{};
    std::memcpy(bad_then_good.data(), bad.data(), bad.size());
    std::memcpy(bad_then_good.data() + bad.size(),
                golden_valid.data(), golden_valid.size());
    require(resync.push_from_isr(bad_then_good.data(),
                                 bad_then_good.size()));
    resync.process(40U);
    const auto recovered = resync.snapshot();
    require(recovered.seen && recovered.seq == 1U);
    require(recovered.checksum_error_count == 1U);
    require(recovered.rejected_count >= 1U);
}

void test_validation_and_per_axis_clamp() noexcept
{
    vision_command_receiver receiver{};

    auto wrong_magic = golden_valid;
    wrong_magic[0U] = 0x00U;
    ingest(receiver, wrong_magic, 1U);
    require(!receiver.snapshot().seen);

    auto wrong_valid = frame_of(1U, 0.0F, 0.0F, 2U);
    ingest(receiver, wrong_valid, 2U);
    require(!receiver.snapshot().seen);

    auto wrong_padding = frame_of(1U, 0.0F, 0.0F, 1U);
    wrong_padding[17U] = 1U;
    write_u32(wrong_padding, 20U,
              vehicle::combined::vision_checksum(
                  wrong_padding.data(), 20U));
    ingest(receiver, wrong_padding, 3U);
    require(!receiver.snapshot().seen);

    auto nan_frame = frame_of(
        1U, std::numeric_limits<float>::quiet_NaN(), 0.0F, 1U);
    ingest(receiver, nan_frame, 4U);
    require(!receiver.snapshot().seen);
    auto infinity_frame = frame_of(
        1U, 0.0F, std::numeric_limits<float>::infinity(), 1U);
    ingest(receiver, infinity_frame, 5U);
    require(!receiver.snapshot().seen);

    const auto clamped_frame = frame_of(1U, 3.0F, -2.0F, 1U);
    ingest(receiver, clamped_frame, 6U);
    const auto clamped = receiver.snapshot();
    require(clamped.seen && clamped.valid);
    require(near(clamped.vx_mps, 0.8F));
    require(near(clamped.vy_mps, -0.8F));
}

void test_sequence_order_and_wrap() noexcept
{
    require(sequence_is_newer(11U, 10U));
    require(!sequence_is_newer(10U, 10U));
    require(!sequence_is_newer(9U, 10U));
    require(sequence_is_newer(0U, 0xFFFFFFFFU));

    vision_command_receiver receiver{};
    ingest(receiver, frame_of(10U, 0.1F, 0.0F, 1U), 10U);
    ingest(receiver, frame_of(10U, 0.2F, 0.0F, 1U), 20U);
    ingest(receiver, frame_of(9U, 0.3F, 0.0F, 1U), 30U);
    auto snapshot = receiver.snapshot();
    require(snapshot.seq == 10U && snapshot.generation == 1U);
    require(snapshot.received_tick == 10U);

    ingest(receiver, frame_of(1000U, 0.4F, 0.0F, 1U), 40U);
    snapshot = receiver.snapshot();
    require(snapshot.seq == 1000U && snapshot.generation == 2U);

    vision_command_receiver wrap{};
    ingest(wrap, frame_of(0xFFFFFFFFU, 0.0F, 0.0F, 1U), 1U);
    ingest(wrap, golden_wrap, 2U);
    require(wrap.snapshot().seq == 0U);
    require(wrap.snapshot().generation == 2U);
}

void test_queue_overflow_invalidates_until_a_new_frame() noexcept
{
    vision_command_receiver receiver{};
    ingest(receiver, golden_valid, 1U);
    require(receiver.snapshot().valid);

    std::array<std::uint8_t, 129U> too_large{};
    require(!receiver.push_from_isr(too_large.data(), too_large.size()));
    receiver.process(2U);
    auto snapshot = receiver.snapshot();
    require(!snapshot.seen && !snapshot.valid);
    require(snapshot.overflow_count == 1U);

    ingest(receiver, golden_stop, 3U);
    snapshot = receiver.snapshot();
    require(snapshot.seen && !snapshot.valid);
    require(snapshot.generation == 2U);
}

void test_auto_gate_requires_every_deadman_and_freshness_condition() noexcept
{
    vision_command_receiver receiver{};
    ingest(receiver, golden_valid, 100U);

    vehicle::combined::vision_auto_gate_input gate{};
    gate.ps2_online = true;
    gate.globally_unlocked = true;
    gate.auto_mode = true;
    gate.l1_held = true;
    gate.entry_generation = 0U;
    gate.command = receiver.snapshot();
    gate.now_tick = 300U;
    gate.chassis_healthy = true;
    require(vehicle::combined::vision_auto_motion_allowed(gate));

    gate.entry_generation = gate.command.generation;
    require(!vehicle::combined::vision_auto_motion_allowed(gate));
    gate.entry_generation = 0U;
    gate.l1_held = false;
    require(!vehicle::combined::vision_auto_motion_allowed(gate));
    gate.l1_held = true;
    gate.now_tick = 301U;
    require(!vehicle::combined::vision_auto_motion_allowed(gate));
    gate.now_tick = 300U;
    gate.command.valid = false;
    require(!vehicle::combined::vision_auto_motion_allowed(gate));
    gate.command.valid = true;
    gate.chassis_healthy = false;
    require(!vehicle::combined::vision_auto_motion_allowed(gate));
    gate.chassis_healthy = true;
    gate.globally_unlocked = false;
    require(!vehicle::combined::vision_auto_motion_allowed(gate));
    gate.globally_unlocked = true;
    gate.ps2_online = false;
    require(!vehicle::combined::vision_auto_motion_allowed(gate));
}

} // namespace

int main()
{
    test_golden_vectors_and_stop();
    test_stream_split_concatenated_garbage_and_resync();
    test_validation_and_per_axis_clamp();
    test_sequence_order_and_wrap();
    test_queue_overflow_invalidates_until_a_new_frame();
    test_auto_gate_requires_every_deadman_and_freshness_condition();
    return EXIT_SUCCESS;
}
