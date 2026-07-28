#include "bsp_usart.hpp"
#include "bsp_usart_backend.hpp"

#include "memory.h"
#include "stm32f4xx_hal.h"
#include "usart.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>

namespace
{

constexpr std::size_t tx_stage_size = 256U;
constexpr std::uintptr_t dma_sram_begin = 0x20000000UL;
constexpr std::uintptr_t dma_sram_end = 0x20020000UL;

struct backend_state
{
    bsp::usart::detail::async_gate tx_gate{};
    std::atomic<bool> tx_in_flight{false};
    bsp::usart::mode active_mode = bsp::usart::mode::block;
    std::uint8_t* rx_buffer = nullptr;
    std::size_t rx_buffer_len = 0U;
    std::size_t rx_cursor = 0U;
    bsp::usart::rx_delivery delivery =
        bsp::usart::rx_delivery::frame_snapshot;
};

static_assert(std::atomic<bool>::is_always_lock_free,
              "UART ISR state requires lock-free atomics");

std::array<backend_state, bsp::usart::port_count> states{};
alignas(4)
std::uint8_t tx_stage[bsp::usart::port_count][tx_stage_size]
    PNX_DMA_BUFFER{};

backend_state* state_of(
    bsp::usart::port selected) noexcept
{
    return selected < bsp::usart::port_count
               ? &states[selected]
               : nullptr;
}

UART_HandleTypeDef* handle_from_id(
    bsp::usart::handle_id id) noexcept
{
    switch (id)
    {
    case bsp::usart::handle_id::usart1:
        return &huart1;
    case bsp::usart::handle_id::usart3:
        return &huart3;
    case bsp::usart::handle_id::usart6:
        return &huart6;
    default:
        return nullptr;
    }
}

UART_HandleTypeDef* handle_of(
    bsp::usart::port selected) noexcept
{
    if (selected >= bsp::usart::port_count ||
        !bsp::usart::configs[selected].enabled)
    {
        return nullptr;
    }
    return handle_from_id(
        bsp::usart::configs[selected].handle);
}

bsp::usart::port port_of(
    UART_HandleTypeDef* handle) noexcept
{
    for (bsp::usart::port selected = 0U;
         selected < bsp::usart::port_count; ++selected)
    {
        if (handle_of(selected) == handle)
        {
            return selected;
        }
    }
    return bsp::usart::port_count;
}

bool dma_accessible(
    const void* buffer, std::size_t len) noexcept
{
    if (buffer == nullptr || len == 0U)
    {
        return false;
    }
    const auto begin =
        reinterpret_cast<std::uintptr_t>(buffer);
    if (len - 1U >
        std::numeric_limits<std::uintptr_t>::max() - begin)
    {
        return false;
    }
    const auto last = begin + len - 1U;
    return begin >= dma_sram_begin && last < dma_sram_end;
}

bool circular_rx(
    UART_HandleTypeDef* handle) noexcept
{
    return handle != nullptr && handle->hdmarx != nullptr &&
           handle->hdmarx->Init.Mode == DMA_CIRCULAR;
}

types::status to_status(
    HAL_StatusTypeDef status) noexcept
{
    if (status == HAL_OK)
    {
        return types::status::ok;
    }
    return status == HAL_BUSY ? types::status::busy
                              : types::status::error;
}

types::status start_dma_rx(
    bsp::usart::port selected, std::uint8_t* buffer,
    std::size_t len,
    bsp::usart::rx_delivery delivery) noexcept
{
    UART_HandleTypeDef* handle = handle_of(selected);
    if (handle == nullptr || !dma_accessible(buffer, len) ||
        len > std::numeric_limits<std::uint16_t>::max())
    {
        return types::status::invalid_arg;
    }

    const HAL_StatusTypeDef status =
        HAL_UARTEx_ReceiveToIdle_DMA(
            handle, buffer, static_cast<std::uint16_t>(len));
    if (status != HAL_OK)
    {
        return to_status(status);
    }
    if (handle->hdmarx != nullptr)
    {
        if (circular_rx(handle) &&
            delivery ==
                bsp::usart::rx_delivery::stream_segments)
        {
            __HAL_DMA_ENABLE_IT(handle->hdmarx, DMA_IT_HT);
        }
        else
        {
            __HAL_DMA_DISABLE_IT(handle->hdmarx, DMA_IT_HT);
        }
    }
    return types::status::ok;
}

void release_tx(
    bsp::usart::port selected, bool completed) noexcept
{
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr ||
        !ctx->tx_in_flight.exchange(
            false, std::memory_order_acq_rel))
    {
        return;
    }
    ctx->tx_gate.release();
    if (completed)
    {
        bsp::usart::detail::tx_complete_from_isr(selected);
    }
}

} // namespace

namespace bsp::usart::detail
{

types::status backend_init(
    port selected, mode selected_mode) noexcept
{
    UART_HandleTypeDef* handle = handle_of(selected);
    backend_state* ctx = state_of(selected);
    if (handle == nullptr || ctx == nullptr)
    {
        return types::status::invalid_arg;
    }
    __HAL_UART_FLUSH_DRREGISTER(handle);
    ctx->active_mode = selected_mode;
    return types::status::ok;
}

types::status backend_transmit(
    port selected, mode selected_mode,
    const std::uint8_t* data, std::size_t len,
    std::uint32_t timeout_ms) noexcept
{
    UART_HandleTypeDef* handle = handle_of(selected);
    backend_state* ctx = state_of(selected);
    if (handle == nullptr || ctx == nullptr ||
        data == nullptr || len == 0U ||
        len > std::numeric_limits<std::uint16_t>::max())
    {
        return types::status::invalid_arg;
    }

    if (selected_mode == mode::block)
    {
        return to_status(HAL_UART_Transmit(
            handle, const_cast<std::uint8_t*>(data),
            static_cast<std::uint16_t>(len), timeout_ms));
    }
    if (len > tx_stage_size)
    {
        return types::status::invalid_arg;
    }
    if (selected_mode == mode::dma &&
        !configs[selected].has_tx_dma)
    {
        return types::status::not_configured;
    }

    async_gate_lease lease(ctx->tx_gate);
    if (!lease.acquired())
    {
        return types::status::busy;
    }
    std::memcpy(tx_stage[selected], data, len);
    ctx->tx_in_flight.store(
        true, std::memory_order_release);
    const HAL_StatusTypeDef status =
        selected_mode == mode::dma
            ? HAL_UART_Transmit_DMA(
                  handle, tx_stage[selected],
                  static_cast<std::uint16_t>(len))
            : HAL_UART_Transmit_IT(
                  handle, tx_stage[selected],
                  static_cast<std::uint16_t>(len));
    if (status != HAL_OK)
    {
        ctx->tx_in_flight.store(
            false, std::memory_order_release);
        return to_status(status);
    }
    lease.hand_off_to_isr();
    return types::status::ok;
}

types::status backend_start_rx(
    port selected, mode selected_mode,
    std::uint8_t* buffer, std::size_t len,
    rx_delivery delivery) noexcept
{
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr || selected_mode != mode::dma ||
        !configs[selected].has_rx_dma)
    {
        return types::status::invalid_arg;
    }
    const types::status status =
        start_dma_rx(selected, buffer, len, delivery);
    if (status != types::status::ok)
    {
        return status;
    }
    ctx->rx_buffer = buffer;
    ctx->rx_buffer_len = len;
    ctx->rx_cursor = 0U;
    ctx->delivery = delivery;
    return types::status::ok;
}

types::status backend_restart_rx(
    port selected) noexcept
{
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr || ctx->rx_buffer == nullptr)
    {
        return types::status::invalid_arg;
    }
    const types::status status = start_dma_rx(
        selected, ctx->rx_buffer, ctx->rx_buffer_len,
        ctx->delivery);
    if (status == types::status::ok)
    {
        ctx->rx_cursor = 0U;
    }
    return status;
}

} // namespace bsp::usart::detail

extern "C" void HAL_UARTEx_RxEventCallback(
    UART_HandleTypeDef* handle, std::uint16_t size)
{
    const bsp::usart::port selected = port_of(handle);
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr || ctx->rx_buffer == nullptr ||
        size > ctx->rx_buffer_len)
    {
        return;
    }

    if (!circular_rx(handle))
    {
        if (HAL_UARTEx_GetRxEventType(handle) !=
            HAL_UART_RXEVENT_HT)
        {
            bsp::usart::detail::rx_from_isr(
                selected, 0U, size);
            (void)bsp::usart::detail::backend_restart_rx(
                selected);
        }
        return;
    }

    const auto event_type =
        HAL_UARTEx_GetRxEventType(handle);
    if (ctx->delivery ==
        bsp::usart::rx_delivery::frame_snapshot)
    {
        if (event_type == HAL_UART_RXEVENT_IDLE &&
            size != 0U)
        {
            bsp::usart::detail::rx_from_isr(
                selected, 0U, size);
        }
        return;
    }

    const auto event =
        event_type == HAL_UART_RXEVENT_TC
            ? bsp::usart::detail::rx_event::
                  transfer_complete
            : event_type == HAL_UART_RXEVENT_HT
                  ? bsp::usart::detail::rx_event::
                        half_transfer
                  : bsp::usart::detail::rx_event::idle;
    const auto update =
        bsp::usart::detail::circular_rx_segments(
            ctx->rx_cursor, size, ctx->rx_buffer_len,
            event);
    if (update.first.len != 0U)
    {
        bsp::usart::detail::rx_from_isr(
            selected, update.first.offset,
            update.first.len);
    }
    if (update.second.len != 0U)
    {
        bsp::usart::detail::rx_from_isr(
            selected, update.second.offset,
            update.second.len);
    }
    ctx->rx_cursor = update.next;
}

extern "C" void HAL_UART_ErrorCallback(
    UART_HandleTypeDef* handle)
{
    const bsp::usart::port selected = port_of(handle);
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr)
    {
        return;
    }
    __HAL_UART_CLEAR_OREFLAG(handle);
    if (handle->gState != HAL_UART_STATE_BUSY_TX &&
        handle->gState != HAL_UART_STATE_BUSY_TX_RX)
    {
        release_tx(selected, false);
    }
    bsp::usart::detail::error_from_isr(selected);
    if (ctx->rx_buffer != nullptr)
    {
        (void)bsp::usart::detail::backend_restart_rx(
            selected);
    }
}

extern "C" void HAL_UART_TxCpltCallback(
    UART_HandleTypeDef* handle)
{
    release_tx(port_of(handle), true);
}

extern "C" void HAL_UART_AbortTransmitCpltCallback(
    UART_HandleTypeDef* handle)
{
    const bsp::usart::port selected = port_of(handle);
    release_tx(selected, false);
    bsp::usart::detail::error_from_isr(selected);
}

extern "C" void HAL_UART_AbortCpltCallback(
    UART_HandleTypeDef* handle)
{
    HAL_UART_AbortTransmitCpltCallback(handle);
}
