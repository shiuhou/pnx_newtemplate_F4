#include "fake_usb.hpp"
#include "usb_tx_completion.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace
{

constexpr std::uint16_t max_transfer_size = 64U;
constexpr std::uint8_t queue_depth = 2U;

struct tx_slot
{
    std::array<std::uint8_t, max_transfer_size> data{};
    std::uint16_t length = 0U;
};

std::atomic<types::status> init_result{types::status::ok};
std::atomic<std::uint32_t> init_call_count{0U};
std::atomic<std::uint32_t> signal_call_count{0U};
bsp::usb::config active_config{};
bsp::usb::runtime_state active_state{};
bsp::usb::runtime_state compatibility_snapshot{};
std::array<tx_slot, queue_depth> tx_queue{};
std::uint8_t tx_head = 0U;
std::uint8_t tx_tail = 0U;
std::uint8_t tx_count = 0U;
bool initialized = false;
bool identity_confirmed = true;
bool write_gate_available = true;
pnx::f407::usb_detail::startup_lifecycle lifecycle{};

void update_queue_state() noexcept
{
    active_state.tx_queue_size = tx_count;
    active_state.pending_write_len =
        tx_count == 0U ? 0U : tx_queue[tx_tail].length;
    active_state.tx_queue_high_water =
        std::max(active_state.tx_queue_high_water, tx_count);
}

} // namespace

namespace host_test::fake_usb
{

void reset() noexcept
{
    init_result.store(types::status::ok, std::memory_order_relaxed);
    init_call_count.store(0U, std::memory_order_relaxed);
    signal_call_count.store(0U, std::memory_order_relaxed);
    active_config = {};
    active_state = {};
    compatibility_snapshot = {};
    tx_queue = {};
    tx_head = 0U;
    tx_tail = 0U;
    tx_count = 0U;
    initialized = false;
    identity_confirmed = true;
    write_gate_available = true;
    lifecycle.reset();
}

void set_init_status(types::status status) noexcept
{
    init_result.store(status, std::memory_order_relaxed);
}

void set_identity_confirmed(bool confirmed) noexcept
{
    identity_confirmed = confirmed;
}

void complete_startup(types::status status) noexcept
{
    if (!initialized)
    {
        return;
    }
    if (status == types::status::ok)
    {
        if (!lifecycle.mark_controller_started() &&
            !lifecycle.controller_active())
        {
            lifecycle.fail();
            active_state.connected = false;
            active_state.link = bsp::usb::link_state::fault;
            ++active_state.error_count;
        }
        return;
    }

    lifecycle.fail();
    active_state.connected = false;
    active_state.link = bsp::usb::link_state::fault;
    active_state.write_busy = false;
    active_state.read_busy = false;
    ++active_state.error_count;
}

void complete_activation(types::status status) noexcept
{
    if (!initialized || !lifecycle.controller_active())
    {
        return;
    }
    if (status == types::status::ok)
    {
        set_connected(true);
        return;
    }
    lifecycle.fail();
    active_state.connected = false;
    active_state.link = bsp::usb::link_state::fault;
    active_state.write_busy = false;
    active_state.read_busy = false;
    ++active_state.error_count;
}

void set_write_gate_available(bool available) noexcept
{
    write_gate_available = available;
}

std::uint32_t init_calls() noexcept
{
    return init_call_count.load(std::memory_order_relaxed);
}

std::uint32_t signal_calls() noexcept
{
    return signal_call_count.load(std::memory_order_relaxed);
}

void set_connected(bool connected) noexcept
{
    if (connected)
    {
        if ((!lifecycle.mark_transport_ready() &&
             !lifecycle.transport_usable()) ||
            lifecycle.faulted())
        {
            return;
        }
        if (!active_state.connected)
        {
            ++active_state.connect_count;
        }
        active_state.connected = true;
        active_state.link = bsp::usb::link_state::connected;
        return;
    }

    lifecycle.disconnect();
    if (active_state.connected)
    {
        ++active_state.disconnect_count;
    }
    active_state.connected = false;
    active_state.link = active_state.initialized
                            ? bsp::usb::link_state::initialized
                            : bsp::usb::link_state::uninitialized;
    active_state.write_busy = false;
    active_state.read_busy = false;
    active_state.tx_drop_count += tx_count;
    tx_head = 0U;
    tx_tail = 0U;
    tx_count = 0U;
    update_queue_state();
}

bool pop_tx(std::array<std::uint8_t, 128>& output,
            std::uint16_t& length) noexcept
{
    length = 0U;
    if (!active_state.connected)
    {
        return false;
    }
    if (tx_count > 0U)
    {
        tx_slot& slot = tx_queue[tx_tail];
        std::memcpy(output.data(), slot.data.data(), slot.length);
        length = slot.length;
        tx_tail = static_cast<std::uint8_t>(
            (tx_tail + 1U) % queue_depth);
        --tx_count;
        update_queue_state();
        active_state.write_busy = true;
        active_state.last_write_requested = length;
        return true;
    }

    if (active_config.fill_tx == nullptr)
    {
        return false;
    }
    ++active_state.fill_count;
    const std::uint16_t filled =
        active_config.fill_tx(
            output.data(), max_transfer_size, active_config.user);
    if (filled == 0U || filled > max_transfer_size)
    {
        return false;
    }
    length = filled;
    active_state.write_busy = true;
    active_state.last_write_requested = filled;
    active_state.pending_write_len = filled;
    return true;
}

void poll_worker() noexcept
{
    std::array<std::uint8_t, 128> output{};
    std::uint16_t length = 0U;
    (void)pop_tx(output, length);
}

void complete_tx(std::uint16_t requested, std::uint16_t actual,
                 types::status status,
                 std::uint32_t native_status) noexcept
{
    if (!active_state.connected || !lifecycle.transport_usable() ||
        !active_state.write_busy)
    {
        return;
    }
    active_state.write_busy = false;
    active_state.pending_write_len =
        tx_count == 0U ? 0U : tx_queue[tx_tail].length;
    active_state.last_write_requested = requested;
    active_state.last_write_actual = actual;
    active_state.last_write_status = native_status;
    if (status == types::status::ok && actual == requested)
    {
        ++active_state.write_count;
        active_state.tx_bytes += actual;
    }
    else
    {
        ++active_state.error_count;
    }
    if (active_config.on_tx_done != nullptr)
    {
        active_config.on_tx_done(
            requested, actual, native_status, active_config.user);
    }
}

void inject_rx(const std::uint8_t* data, std::uint16_t length,
               types::status status,
               std::uint32_t native_status) noexcept
{
    active_state.read_busy = false;
    active_state.last_read_status = native_status;
    active_state.last_read_len = length;
    if (status != types::status::ok)
    {
        ++active_state.error_count;
        return;
    }
    if (!active_state.connected || data == nullptr ||
        length < active_config.min_rx_size)
    {
        return;
    }
    ++active_state.read_count;
    active_state.rx_bytes += length;
    if (active_config.on_rx != nullptr)
    {
        active_config.on_rx(data, length, active_config.user);
    }
}

} // namespace host_test::fake_usb

namespace bsp::usb
{

types::status init(const config& cfg) noexcept
{
    if (initialized)
    {
        if (!pnx::f407::usb_detail::same_config(
                active_config, cfg))
        {
            return types::status::invalid_arg;
        }
        return lifecycle.faulted() ? types::status::error
                                   : types::status::ok;
    }
    if (cfg.period_ticks == 0U || cfg.max_tx_size == 0U)
    {
        return types::status::invalid_arg;
    }

    active_config = cfg;
    active_state = {};
    tx_head = 0U;
    tx_tail = 0U;
    tx_count = 0U;
    init_call_count.fetch_add(1U, std::memory_order_relaxed);
    if (!identity_confirmed)
    {
        lifecycle.fail();
        active_state.link = link_state::fault;
        ++active_state.error_count;
        return types::status::not_configured;
    }
    const types::status result =
        init_result.load(std::memory_order_relaxed);
    if (result != types::status::ok)
    {
        lifecycle.fail();
        active_state.link = link_state::fault;
        ++active_state.error_count;
        return result;
    }

    initialized = true;
    lifecycle.schedule();
    active_state.initialized = true;
    active_state.link = link_state::initialized;
    return types::status::ok;
}

write_result write(const std::uint8_t* data, std::size_t len) noexcept
{
    if (data == nullptr || len == 0U || len > max_transfer_size)
    {
        return {types::status::invalid_arg, 0U};
    }
    if (!initialized || !active_state.connected ||
        !lifecycle.transport_usable())
    {
        return {types::status::not_configured, 0U};
    }
    if (!write_gate_available)
    {
        return {types::status::busy, 0U};
    }
    if (len > active_config.max_tx_size)
    {
        return {types::status::invalid_arg, 0U};
    }
    if (tx_count >= queue_depth)
    {
        ++active_state.tx_drop_count;
        return {types::status::busy, 0U};
    }

    tx_slot& slot = tx_queue[tx_head];
    std::memcpy(slot.data.data(), data, len);
    slot.length = static_cast<std::uint16_t>(len);
    tx_head = static_cast<std::uint8_t>(
        (tx_head + 1U) % queue_depth);
    ++tx_count;
    update_queue_state();
    signal_call_count.fetch_add(1U, std::memory_order_relaxed);
    return {types::status::ok, static_cast<std::uint16_t>(len)};
}

bool connected() noexcept
{
    return active_state.connected;
}

runtime_state snapshot() noexcept
{
    return active_state;
}

capabilities get_capabilities() noexcept
{
    capabilities result{};
    result.device_mode = true;
    result.high_speed = false;
    result.supports_vbus_sense = false;
    result.max_packet_size = 64U;
    result.max_transfer_size = max_transfer_size;
    result.tx_queue_depth = queue_depth;
    return result;
}

const runtime_state& state() noexcept
{
    compatibility_snapshot = active_state;
    return compatibility_snapshot;
}

} // namespace bsp::usb
