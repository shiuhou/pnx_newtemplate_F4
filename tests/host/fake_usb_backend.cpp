#include "fake_usb_backend.hpp"

#include "bsp_usb_backend.hpp"

#include <atomic>

namespace
{

std::atomic<types::status> init_result{types::status::ok};
std::atomic<std::uint32_t> init_call_count{0U};
std::atomic<std::uint32_t> signal_call_count{0U};
std::atomic<bool> connected_value{false};

} // namespace

namespace host_test::fake_usb
{

void reset() noexcept
{
    init_result.store(types::status::ok, std::memory_order_relaxed);
    init_call_count.store(0U, std::memory_order_relaxed);
    signal_call_count.store(0U, std::memory_order_relaxed);
    connected_value.store(false, std::memory_order_relaxed);
}

void set_init_status(types::status status) noexcept
{
    init_result.store(status, std::memory_order_relaxed);
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
    connected_value.store(connected, std::memory_order_relaxed);
    bsp::usb::detail::connection_from_backend(connected);
}

bool pop_tx(std::array<std::uint8_t, 128>& output,
            std::uint16_t& length) noexcept
{
    return bsp::usb::detail::prepare_tx_for_backend(
        output.data(), output.size(), length);
}

void complete_tx(std::uint16_t requested, std::uint16_t actual,
                 types::status status,
                 std::uint32_t native_status) noexcept
{
    bsp::usb::detail::transmit_complete_from_backend(
        requested, actual, status, native_status);
}

void inject_rx(const std::uint8_t* data, std::uint16_t length,
               types::status status,
               std::uint32_t native_status) noexcept
{
    bsp::usb::detail::receive_from_backend(
        data, length, status, native_status);
}

} // namespace host_test::fake_usb

namespace bsp::usb::detail
{

types::status backend_init(const backend_config&) noexcept
{
    init_call_count.fetch_add(1U, std::memory_order_relaxed);
    return init_result.load(std::memory_order_relaxed);
}

bool backend_connected() noexcept
{
    return connected_value.load(std::memory_order_relaxed);
}

void backend_signal_tx() noexcept
{
    signal_call_count.fetch_add(1U, std::memory_order_relaxed);
}

capabilities backend_capabilities() noexcept
{
    capabilities result{};
    result.device_mode = true;
    result.high_speed = false;
    result.supports_vbus_sense = false;
    result.max_packet_size = 64U;
    result.max_transfer_size = 128U;
    result.tx_queue_depth = 99U;
    return result;
}

bool backend_lock() noexcept
{
    return true;
}

void backend_unlock() noexcept
{
}

} // namespace bsp::usb::detail
