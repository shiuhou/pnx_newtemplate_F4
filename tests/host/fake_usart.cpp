#include "bsp_usart.hpp"

#include <array>
#include <limits>

namespace
{

struct port_state
{
    bool initialized = false;
    bsp::usart::mode active_mode = bsp::usart::mode::block;
    bsp::usart::rx_handler handler = nullptr;
    void* user_data = nullptr;
    bsp::usart::notify_handler notify = nullptr;
    void* notify_user_data = nullptr;
    bsp::usart::rx_delivery delivery =
        bsp::usart::rx_delivery::frame_snapshot;
    std::uint8_t* rx_buffer = nullptr;
    std::size_t rx_buffer_len = 0U;
    bsp::usart::telemetry telemetry{};
};

std::array<std::uint8_t*, bsp::usart::port_count> rx_buffers{};
std::array<std::size_t, bsp::usart::port_count> rx_lengths{};
std::array<port_state, bsp::usart::port_count> port_states{};
types::status transmit_result = types::status::ok;
types::status restart_result = types::status::ok;

port_state* state_of(bsp::usart::port selected) noexcept
{
    return selected < bsp::usart::port_count
               ? &port_states[selected]
               : nullptr;
}

const bsp::usart::port_config* config_of(
    bsp::usart::port selected) noexcept
{
    return selected < bsp::usart::port_count
               ? &bsp::usart::configs[selected]
               : nullptr;
}

void deliver_rx(bsp::usart::port selected, std::size_t offset,
                std::size_t len) noexcept
{
    port_state* ctx = state_of(selected);
    if (ctx == nullptr || ctx->rx_buffer == nullptr ||
        offset > ctx->rx_buffer_len ||
        len > ctx->rx_buffer_len - offset)
    {
        return;
    }
    ++ctx->telemetry.rx_count;
    ctx->telemetry.last_rx_len = static_cast<std::uint32_t>(len);
    const bsp::usart::rx_frame frame{ctx->rx_buffer + offset, len};
    if (ctx->notify != nullptr)
    {
        ctx->notify(ctx->notify_user_data);
    }
    if (ctx->handler != nullptr)
    {
        ctx->handler(selected, frame, ctx->user_data);
    }
}

} // namespace

namespace host_test::fake_usart
{

void reset() noexcept
{
    rx_buffers = {};
    rx_lengths = {};
    port_states = {};
    transmit_result = types::status::ok;
    restart_result = types::status::ok;
}

void set_transmit_result(types::status result) noexcept
{
    transmit_result = result;
}

void set_restart_result(types::status result) noexcept
{
    restart_result = result;
}

void receive(
    bsp::usart::port selected, std::size_t len) noexcept
{
    if (selected < bsp::usart::port_count &&
        rx_buffers[selected] != nullptr &&
        len <= rx_lengths[selected])
    {
        deliver_rx(selected, 0U, len);
    }
}

} // namespace host_test::fake_usart

namespace bsp::usart
{

bool port_enabled(port selected) noexcept
{
    const port_config* cfg = config_of(selected);
    return cfg != nullptr && cfg->enabled;
}

types::status init(port selected, mode selected_mode)
{
    port_state* ctx = state_of(selected);
    if (ctx == nullptr)
    {
        return types::status::invalid_arg;
    }
    if (!port_enabled(selected))
    {
        return types::status::not_configured;
    }
    if (ctx->initialized)
    {
        return ctx->active_mode == selected_mode ? types::status::ok
                                                 : types::status::invalid_arg;
    }
    ctx->active_mode = selected_mode;
    ctx->initialized = true;
    return types::status::ok;
}

types::status transmit(port selected, const std::uint8_t* data,
                       std::size_t len, std::uint32_t)
{
    port_state* ctx = state_of(selected);
    if (ctx == nullptr || data == nullptr || len == 0U ||
        len > std::numeric_limits<std::uint16_t>::max())
    {
        return types::status::invalid_arg;
    }
    if (!port_enabled(selected))
    {
        return types::status::not_configured;
    }
    if (!ctx->initialized)
    {
        return types::status::error;
    }

    const types::status result = transmit_result;
    if (result == types::status::ok)
    {
        ++ctx->telemetry.tx_count;
    }
    else if (result == types::status::busy)
    {
        ++ctx->telemetry.busy_count;
    }
    else
    {
        ++ctx->telemetry.error_count;
    }
    return result;
}

types::status start_rx_to_idle(port selected, std::uint8_t* buffer,
                               std::size_t len, rx_handler handler,
                               void* user_data, notify_handler notify,
                               void* notify_user_data,
                               rx_delivery delivery)
{
    port_state* ctx = state_of(selected);
    if (ctx == nullptr || buffer == nullptr || len == 0U ||
        len > std::numeric_limits<std::uint16_t>::max() ||
        !ctx->initialized)
    {
        return types::status::invalid_arg;
    }
    if (ctx->rx_buffer != nullptr &&
        (ctx->rx_buffer != buffer || ctx->rx_buffer_len != len ||
         ctx->handler != handler || ctx->user_data != user_data ||
         ctx->notify != notify ||
         ctx->notify_user_data != notify_user_data ||
         ctx->delivery != delivery))
    {
        return types::status::busy;
    }
    if (ctx->active_mode != mode::dma)
    {
        ++ctx->telemetry.error_count;
        return types::status::invalid_arg;
    }

    ctx->rx_buffer = buffer;
    ctx->rx_buffer_len = len;
    ctx->handler = handler;
    ctx->user_data = user_data;
    ctx->notify = notify;
    ctx->notify_user_data = notify_user_data;
    ctx->delivery = delivery;
    rx_buffers[selected] = buffer;
    rx_lengths[selected] = len;
    return types::status::ok;
}

types::status restart_rx(port selected)
{
    port_state* ctx = state_of(selected);
    if (ctx == nullptr || !ctx->initialized || ctx->rx_buffer == nullptr)
    {
        return types::status::invalid_arg;
    }
    const types::status result = restart_result;
    if (result == types::status::busy)
    {
        ++ctx->telemetry.busy_count;
    }
    else if (result != types::status::ok)
    {
        ++ctx->telemetry.error_count;
    }
    return result;
}

telemetry snapshot(port selected) noexcept
{
    const port_state* ctx = state_of(selected);
    return ctx != nullptr ? ctx->telemetry : telemetry{};
}

} // namespace bsp::usart
