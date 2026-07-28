#include "fake_can_backend.hpp"

#include <algorithm>
#include <atomic>

namespace
{

std::atomic<std::uint32_t> tick_value{0U};
std::atomic<std::uint32_t> recover_call_count{0U};
std::atomic<std::uint32_t> transmit_call_count{0U};
std::atomic<types::status> recover_result{types::status::ok};
std::atomic<bool> saw_recovering{false};
host_test::fake_can::tx_record last_tx{};

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
    last_tx = {};
}

void set_tick(std::uint32_t tick) noexcept
{
    tick_value.store(tick, std::memory_order_relaxed);
}

void set_recover_status(types::status status) noexcept
{
    recover_result.store(status, std::memory_order_relaxed);
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

namespace bsp::can::detail
{

types::status backend_init(bus, bus_type) noexcept
{
    return types::status::ok;
}

types::status backend_transmit(bus selected, std::uint32_t id,
                               const std::uint8_t* data,
                               std::uint16_t len) noexcept
{
    transmit_call_count.fetch_add(1U, std::memory_order_relaxed);
    last_tx = {};
    last_tx.bus = selected;
    last_tx.id = id;
    last_tx.len = len;
    if (data != nullptr)
    {
        std::copy_n(data, std::min<std::size_t>(len, last_tx.data.size()),
                    last_tx.data.begin());
    }
    return types::status::ok;
}

types::status backend_recover(bus selected) noexcept
{
    recover_call_count.fetch_add(1U, std::memory_order_relaxed);
    saw_recovering.store(
        snapshot(selected).bus_state == state::recovering,
        std::memory_order_relaxed);
    return recover_result.load(std::memory_order_relaxed);
}

std::uint32_t backend_tick_now() noexcept
{
    return tick_value.load(std::memory_order_relaxed);
}

std::uint32_t backend_enter_critical() noexcept
{
    return 0U;
}

void backend_exit_critical(std::uint32_t) noexcept
{
}

} // namespace bsp::can::detail
