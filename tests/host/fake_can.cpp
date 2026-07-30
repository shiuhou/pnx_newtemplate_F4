#include "fake_can.hpp"

#include <algorithm>
#include <array>
#include <atomic>

namespace
{

struct rx_slot
{
    std::atomic<bsp::can::rx_handler> handler{nullptr};
    std::atomic<void*> user_data{nullptr};
};

struct telemetry_state
{
    std::atomic<std::uint32_t> rx_count{0U};
    std::atomic<std::uint32_t> tx_count{0U};
    std::atomic<std::uint32_t> last_id{0U};
    std::atomic<std::uint32_t> last_tick{0U};
    std::atomic<std::uint32_t> error_count{0U};
    std::atomic<std::uint32_t> bus_off_count{0U};
    std::atomic<std::uint32_t> drop_count{0U};
    std::atomic<std::uint32_t> bus_state{
        static_cast<std::uint32_t>(bsp::can::state::stopped)};
};

std::array<bsp::can::bus_type, bsp::can::bus_count> active_types{};
std::array<bool, bsp::can::bus_count> initialized{};
std::array<std::array<rx_slot, bsp::can::max_rx_callbacks>,
           bsp::can::bus_count>
    rx_slots{};
std::array<telemetry_state, bsp::can::bus_count> telemetry_states{};
std::array<std::atomic<bool>, bsp::can::bus_count> recovery_in_progress{};
std::atomic<std::uint32_t> fault_epoch{0U};

std::atomic<std::uint32_t> tick_value{0U};
std::atomic<std::uint32_t> recover_call_count{0U};
std::atomic<std::uint32_t> transmit_call_count{0U};
std::atomic<types::status> recover_result{types::status::ok};
std::atomic<bool> saw_recovering{false};
host_test::fake_can::tx_record last_tx{};

std::size_t index_of(bsp::can::bus selected) noexcept
{
    const auto index = static_cast<std::size_t>(selected);
    return index < bsp::can::bus_count ? index : bsp::can::bus_count;
}

const bsp::can::bus_config* config_of(std::size_t index) noexcept
{
    return index < bsp::can::bus_count ? &bsp::can::configs[index] : nullptr;
}

bool valid_transmit(std::size_t index, const std::uint8_t* data,
                    std::uint16_t len) noexcept
{
    return index < bsp::can::bus_count && data != nullptr && len != 0U;
}

types::status record_transmit(bsp::can::bus selected, std::uint32_t id,
                              const std::uint8_t* data,
                              std::uint16_t len) noexcept
{
    transmit_call_count.fetch_add(1U, std::memory_order_relaxed);
    last_tx = {};
    last_tx.bus = selected;
    last_tx.id = id;
    last_tx.len = len;
    std::copy_n(data, std::min<std::size_t>(len, last_tx.data.size()),
                last_tx.data.begin());
    return types::status::ok;
}

} // namespace

namespace host_test::fake_can
{

void reset() noexcept
{
    tick_value.store(0U, std::memory_order_relaxed);
    recover_call_count.store(0U, std::memory_order_relaxed);
    transmit_call_count.store(0U, std::memory_order_relaxed);
    recover_result.store(types::status::ok, std::memory_order_relaxed);
    saw_recovering.store(false, std::memory_order_relaxed);
    fault_epoch.store(0U, std::memory_order_relaxed);
    last_tx = {};

    for (std::size_t index = 0; index < bsp::can::bus_count; ++index)
    {
        active_types[index] = bsp::can::bus_type::classic;
        initialized[index] = false;
        recovery_in_progress[index].store(false, std::memory_order_relaxed);
        telemetry_state& stats = telemetry_states[index];
        stats.rx_count.store(0U, std::memory_order_relaxed);
        stats.tx_count.store(0U, std::memory_order_relaxed);
        stats.last_id.store(0U, std::memory_order_relaxed);
        stats.last_tick.store(0U, std::memory_order_relaxed);
        stats.error_count.store(0U, std::memory_order_relaxed);
        stats.bus_off_count.store(0U, std::memory_order_relaxed);
        stats.drop_count.store(0U, std::memory_order_relaxed);
        stats.bus_state.store(
            static_cast<std::uint32_t>(bsp::can::state::stopped),
            std::memory_order_relaxed);
        for (auto& slot : rx_slots[index])
        {
            slot.handler.store(nullptr, std::memory_order_relaxed);
            slot.user_data.store(nullptr, std::memory_order_relaxed);
        }
    }
}

void set_tick(std::uint32_t tick) noexcept
{
    tick_value.store(tick, std::memory_order_relaxed);
}

void set_recover_status(types::status status) noexcept
{
    recover_result.store(status, std::memory_order_relaxed);
}

void inject_error(bsp::can::bus selected, bsp::can::state next_state,
                  std::uint32_t tick) noexcept
{
    const std::size_t index = index_of(selected);
    if (index >= bsp::can::bus_count)
    {
        return;
    }
    telemetry_state& stats = telemetry_states[index];
    stats.last_tick.store(tick, std::memory_order_relaxed);
    stats.bus_state.store(
        static_cast<std::uint32_t>(next_state), std::memory_order_relaxed);
    if (next_state == bsp::can::state::bus_off)
    {
        stats.bus_off_count.fetch_add(1U, std::memory_order_relaxed);
    }
    stats.error_count.fetch_add(1U, std::memory_order_release);
    fault_epoch.fetch_add(1U, std::memory_order_release);
}

std::uint32_t recover_calls() noexcept
{
    return recover_call_count.load(std::memory_order_relaxed);
}

std::uint32_t transmit_calls() noexcept
{
    return transmit_call_count.load(std::memory_order_relaxed);
}

bool observed_recovering_state() noexcept
{
    return saw_recovering.load(std::memory_order_relaxed);
}

tx_record last_transmit() noexcept
{
    return last_tx;
}

} // namespace host_test::fake_can

namespace bsp::can
{

bool bus_enabled(std::size_t index) noexcept
{
    const bus_config* cfg = config_of(index);
    return cfg != nullptr && cfg->enabled;
}

bus_type configured_bus_type(std::size_t index) noexcept
{
    const bus_config* cfg = config_of(index);
    return cfg != nullptr ? cfg->type : bus_type::classic;
}

id_type filter_id_type_of(std::size_t index) noexcept
{
    const bus_config* cfg = config_of(index);
    return cfg != nullptr ? cfg->filter_id_type : id_type::standard;
}

types::status init(bus selected, bus_type type)
{
    const std::size_t index = index_of(selected);
    if (index >= bus_count)
    {
        return types::status::invalid_arg;
    }
    if (!bus_enabled(index))
    {
        return types::status::not_configured;
    }
    if (initialized[index])
    {
        return active_types[index] == type ? types::status::ok
                                           : types::status::invalid_arg;
    }
    if (configured_bus_type(index) == bus_type::classic &&
        type == bus_type::fd)
    {
        return types::status::not_configured;
    }

    active_types[index] = type;
    initialized[index] = true;
    telemetry_states[index].bus_state.store(
        static_cast<std::uint32_t>(state::active),
        std::memory_order_release);
    return types::status::ok;
}

types::status recover(bus selected)
{
    const std::size_t index = index_of(selected);
    if (index >= bus_count)
    {
        return types::status::invalid_arg;
    }
    if (!bus_enabled(index))
    {
        return types::status::not_configured;
    }
    if (!initialized[index])
    {
        return types::status::error;
    }

    telemetry_state& stats = telemetry_states[index];
    if (static_cast<state>(
            stats.bus_state.load(std::memory_order_acquire)) == state::active)
    {
        return types::status::ok;
    }

    bool expected = false;
    if (!recovery_in_progress[index].compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
    {
        return types::status::busy;
    }

    const state current = static_cast<state>(
        stats.bus_state.load(std::memory_order_acquire));
    if (current != state::bus_off)
    {
        recovery_in_progress[index].store(false, std::memory_order_release);
        return current == state::active ? types::status::ok
                                        : types::status::error;
    }
    stats.bus_state.store(
        static_cast<std::uint32_t>(state::recovering),
        std::memory_order_release);

    recover_call_count.fetch_add(1U, std::memory_order_relaxed);
    saw_recovering.store(
        snapshot(selected).bus_state == state::recovering,
        std::memory_order_relaxed);
    const types::status status =
        recover_result.load(std::memory_order_relaxed);
    const bool owns_transition =
        static_cast<state>(
            stats.bus_state.load(std::memory_order_acquire)) ==
        state::recovering;
    if (status == types::status::ok && owns_transition)
    {
        stats.bus_state.store(
            static_cast<std::uint32_t>(state::active),
            std::memory_order_release);
    }
    else if (owns_transition)
    {
        stats.bus_state.store(
            static_cast<std::uint32_t>(state::fault),
            std::memory_order_release);
        stats.error_count.fetch_add(1U, std::memory_order_release);
        fault_epoch.fetch_add(1U, std::memory_order_release);
    }
    recovery_in_progress[index].store(false, std::memory_order_release);
    return status == types::status::ok && owns_transition
               ? types::status::ok
               : types::status::error;
}

types::status transmit(bus selected, std::uint32_t id,
                       const std::uint8_t* data, std::uint16_t len)
{
    const std::size_t index = index_of(selected);
    if (!valid_transmit(index, data, len))
    {
        return types::status::invalid_arg;
    }
    if (!bus_enabled(index))
    {
        return types::status::not_configured;
    }
    if (!initialized[index])
    {
        return types::status::error;
    }
    if (static_cast<state>(
            telemetry_states[index].bus_state.load(
                std::memory_order_acquire)) == state::bus_off)
    {
        const types::status recovery_status = recover(selected);
        if (recovery_status != types::status::ok)
        {
            return recovery_status;
        }
    }

    const types::status status = record_transmit(selected, id, data, len);
    if (status == types::status::ok)
    {
        telemetry_states[index].tx_count.fetch_add(
            1U, std::memory_order_relaxed);
    }
    return status;
}

types::status transmit_if_healthy(
    bus selected, std::uint32_t id, const std::uint8_t* data,
    std::uint16_t len, std::uint32_t expected_error_count,
    std::uint32_t expected_drop_count,
    std::uint32_t expected_fault_epoch)
{
    const std::size_t index = index_of(selected);
    if (!valid_transmit(index, data, len))
    {
        return types::status::invalid_arg;
    }
    if (!bus_enabled(index))
    {
        return types::status::not_configured;
    }
    if (!initialized[index])
    {
        return types::status::error;
    }

    telemetry_state& stats = telemetry_states[index];
    const bool healthy =
        stats.error_count.load(std::memory_order_acquire) ==
            expected_error_count &&
        stats.drop_count.load(std::memory_order_acquire) ==
            expected_drop_count &&
        fault_epoch.load(std::memory_order_acquire) ==
            expected_fault_epoch &&
        static_cast<state>(
            stats.bus_state.load(std::memory_order_acquire)) ==
            state::active;
    const types::status status =
        healthy ? record_transmit(selected, id, data, len)
                : types::status::error;
    if (status == types::status::ok)
    {
        stats.tx_count.fetch_add(1U, std::memory_order_relaxed);
    }
    return status;
}

types::status register_rx_handler(bus selected, rx_handler handler,
                                  void* user_data)
{
    const std::size_t index = index_of(selected);
    if (index >= bus_count || handler == nullptr)
    {
        return types::status::invalid_arg;
    }
    if (!bus_enabled(index))
    {
        return types::status::not_configured;
    }

    for (auto& slot : rx_slots[index])
    {
        if (slot.handler.load(std::memory_order_acquire) == nullptr)
        {
            slot.user_data.store(user_data, std::memory_order_relaxed);
            slot.handler.store(handler, std::memory_order_release);
            return types::status::ok;
        }
    }
    return types::status::busy;
}

void unregister_rx_handlers(bus selected)
{
    const std::size_t index = index_of(selected);
    if (index >= bus_count)
    {
        return;
    }
    for (auto& slot : rx_slots[index])
    {
        slot.handler.store(nullptr, std::memory_order_release);
    }
}

telemetry snapshot(bus selected) noexcept
{
    telemetry result{};
    const std::size_t index = index_of(selected);
    if (index >= bus_count)
    {
        result.bus_state = state::fault;
        return result;
    }

    const telemetry_state& source = telemetry_states[index];
    result.rx_count = source.rx_count.load(std::memory_order_relaxed);
    result.tx_count = source.tx_count.load(std::memory_order_relaxed);
    result.last_id = source.last_id.load(std::memory_order_relaxed);
    result.last_tick = source.last_tick.load(std::memory_order_relaxed);
    result.error_count = source.error_count.load(std::memory_order_acquire);
    result.bus_off_count =
        source.bus_off_count.load(std::memory_order_relaxed);
    result.drop_count = source.drop_count.load(std::memory_order_acquire);
    result.fault_epoch = fault_epoch.load(std::memory_order_acquire);
    result.bus_state = static_cast<state>(
        source.bus_state.load(std::memory_order_acquire));
    return result;
}

std::uint32_t time_now() noexcept
{
    return tick_value.load(std::memory_order_relaxed);
}

} // namespace bsp::can
