#include "bsp_usart.hpp"
#include "bsp_usart_backend.hpp"

#include "dma.h"
#include "memory.h"
#include "stm32f4xx_hal.h"
#include "usart.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>

extern "C" {
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;
}

namespace
{

constexpr std::size_t dma_tx_stage_size = 256;
constexpr std::uintptr_t sram_start = 0x20000000UL;
constexpr std::uintptr_t sram_end = 0x20020000UL;

struct backend_state
{
    bsp::usart::detail::async_gate tx_gate{};
    std::atomic<bool> tx_in_flight{false};
    bsp::usart::mode active_mode = bsp::usart::mode::block;
    std::uint8_t* rx_buffer = nullptr;
    std::size_t rx_buffer_len = 0;
    std::size_t rx_cursor = 0;
    bsp::usart::rx_delivery rx_delivery =
        bsp::usart::rx_delivery::frame_snapshot;
};

static_assert(std::atomic<bool>::is_always_lock_free,
              "UART ISR state requires lock-free atomics");

std::array<backend_state, bsp::usart::port_count> states{};
alignas(4)
std::uint8_t tx_dma_stage[bsp::usart::port_count][dma_tx_stage_size]
    RAM_D1_BSS{};

backend_state* state_of(bsp::usart::port selected) noexcept
{
    return selected < bsp::usart::port_count ? &states[selected] : nullptr;
}

UART_HandleTypeDef* handle_from_id(bsp::usart::handle_id id) noexcept
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

UART_HandleTypeDef* handle_of(bsp::usart::port selected) noexcept
{
    if (selected >= bsp::usart::port_count ||
        !bsp::usart::configs[selected].enabled)
    {
        return nullptr;
    }
    return handle_from_id(bsp::usart::configs[selected].handle);
}

bsp::usart::port port_of(UART_HandleTypeDef* handle) noexcept
{
    for (bsp::usart::port selected = 0;
         selected < bsp::usart::port_count; ++selected)
    {
        if (handle_of(selected) == handle)
        {
            return selected;
        }
    }
    return bsp::usart::port_count;
}

DMA_HandleTypeDef* rx_dma_from_id(bsp::usart::handle_id id) noexcept
{
    switch (id)
    {
    case bsp::usart::handle_id::usart1:
        return &hdma_usart1_rx;
    case bsp::usart::handle_id::usart3:
        return &hdma_usart3_rx;
    case bsp::usart::handle_id::usart6:
        return &hdma_usart6_rx;
    default:
        return nullptr;
    }
}

DMA_HandleTypeDef* tx_dma_from_id(bsp::usart::handle_id id) noexcept
{
    switch (id)
    {
    case bsp::usart::handle_id::usart1:
        return &hdma_usart1_tx;
    case bsp::usart::handle_id::usart6:
        return &hdma_usart6_tx;
    default:
        return nullptr;
    }
}

void enable_dma_irq(DMA_HandleTypeDef* dma) noexcept
{
    if (dma != nullptr)
    {
        __HAL_DMA_DISABLE_IT(dma, DMA_IT_HT);
        __HAL_DMA_ENABLE_IT(dma, DMA_IT_TC);
    }
}

bool dma_accessible(const void* buffer, std::size_t len) noexcept
{
    if (buffer == nullptr || len == 0U)
    {
        return false;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(buffer);
    if (len - 1U >
        std::numeric_limits<std::uintptr_t>::max() - begin)
    {
        return false;
    }
    const auto last = begin + len - 1U;
    return begin >= sram_start && last < sram_end;
}

bool rx_dma_is_circular(UART_HandleTypeDef* handle) noexcept
{
    return handle != nullptr && handle->hdmarx != nullptr &&
           handle->hdmarx->Init.Mode == DMA_CIRCULAR;
}

types::status hal_status(HAL_StatusTypeDef status) noexcept
{
    if (status == HAL_OK)
    {
        return types::status::ok;
    }
    return status == HAL_BUSY ? types::status::busy
                              : types::status::error;
}

types::status start_dma_rx(bsp::usart::port selected,
                           std::uint8_t* buffer, std::size_t len,
                           bsp::usart::rx_delivery delivery) noexcept
{
    UART_HandleTypeDef* handle = handle_of(selected);
    if (handle == nullptr || !dma_accessible(buffer, len) ||
        len > std::numeric_limits<std::uint16_t>::max())
    {
        return types::status::invalid_arg;
    }
    const HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(
        handle, buffer, static_cast<std::uint16_t>(len));
    if (status != HAL_OK)
    {
        return hal_status(status);
    }
    if (handle->hdmarx != nullptr)
    {
        if (rx_dma_is_circular(handle) &&
            delivery == bsp::usart::rx_delivery::stream_segments)
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

bool finish_tx_from_isr(backend_state& ctx, bool completed) noexcept
{
    if (!ctx.tx_in_flight.exchange(false, std::memory_order_acq_rel))
    {
        return false;
    }
    ctx.tx_gate.release();
    if (completed)
    {
        const auto selected = static_cast<bsp::usart::port>(
            &ctx - states.data());
        bsp::usart::detail::tx_complete_from_isr(selected);
    }
    return true;
}

bool tx_error_ended_transfer(UART_HandleTypeDef* handle,
                             const backend_state& ctx) noexcept
{
    if (handle == nullptr)
    {
        return false;
    }
    const bool hal_tx_state_busy =
        handle->gState == HAL_UART_STATE_BUSY_TX ||
        handle->gState == HAL_UART_STATE_BUSY_TX_RX;
    const bool dma_tx_request_active =
        ctx.active_mode == bsp::usart::mode::dma &&
        HAL_IS_BIT_SET(handle->Instance->CR3, USART_CR3_DMAT);
    const bool dma_error_reported =
        (handle->ErrorCode & HAL_UART_ERROR_DMA) != 0U;
    return bsp::usart::detail::async_tx_error_releases_gate(
        ctx.tx_in_flight.load(std::memory_order_acquire),
        hal_tx_state_busy, dma_tx_request_active,
        dma_error_reported);
}

} // namespace

namespace bsp::usart::detail
{

types::status backend_init(port selected, mode selected_mode) noexcept
{
    UART_HandleTypeDef* handle = handle_of(selected);
    backend_state* ctx = state_of(selected);
    if (handle == nullptr || ctx == nullptr)
    {
        return types::status::invalid_arg;
    }
    if (selected_mode == mode::dma)
    {
        const port_config& cfg = configs[selected];
        if (cfg.has_rx_dma)
        {
            enable_dma_irq(rx_dma_from_id(cfg.handle));
        }
        if (cfg.has_tx_dma)
        {
            enable_dma_irq(tx_dma_from_id(cfg.handle));
        }
    }
    __HAL_UART_FLUSH_DRREGISTER(handle);
    ctx->active_mode = selected_mode;
    return types::status::ok;
}

types::status backend_transmit(port selected, mode selected_mode,
                               const std::uint8_t* data, std::size_t len,
                               std::uint32_t timeout_ms) noexcept
{
    UART_HandleTypeDef* handle = handle_of(selected);
    backend_state* ctx = state_of(selected);
    if (handle == nullptr || ctx == nullptr || data == nullptr ||
        len == 0U ||
        len > std::numeric_limits<std::uint16_t>::max())
    {
        return types::status::invalid_arg;
    }
    if (selected_mode == mode::dma)
    {
        if (!configs[selected].has_tx_dma)
        {
            return types::status::not_configured;
        }
        detail::async_gate_lease lease(ctx->tx_gate);
        if (!lease.acquired())
        {
            return types::status::busy;
        }
        if (len > dma_tx_stage_size)
        {
            return types::status::invalid_arg;
        }
        std::memcpy(tx_dma_stage[selected], data, len);
        ctx->tx_in_flight.store(true, std::memory_order_release);
        const HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(
            handle, tx_dma_stage[selected],
            static_cast<std::uint16_t>(len));
        if (status != HAL_OK)
        {
            ctx->tx_in_flight.store(false, std::memory_order_release);
            return hal_status(status);
        }
        lease.hand_off_to_isr();
        return types::status::ok;
    }
    if (selected_mode == mode::it)
    {
        detail::async_gate_lease lease(ctx->tx_gate);
        if (!lease.acquired())
        {
            return types::status::busy;
        }
        if (len > dma_tx_stage_size)
        {
            return types::status::invalid_arg;
        }
        std::memcpy(tx_dma_stage[selected], data, len);
        ctx->tx_in_flight.store(true, std::memory_order_release);
        const HAL_StatusTypeDef status = HAL_UART_Transmit_IT(
            handle, tx_dma_stage[selected],
            static_cast<std::uint16_t>(len));
        if (status != HAL_OK)
        {
            ctx->tx_in_flight.store(false, std::memory_order_release);
            return hal_status(status);
        }
        lease.hand_off_to_isr();
        return types::status::ok;
    }
    return HAL_UART_Transmit(
               handle, const_cast<std::uint8_t*>(data),
               static_cast<std::uint16_t>(len), timeout_ms) == HAL_OK
               ? types::status::ok
               : types::status::error;
}

types::status backend_start_rx(port selected, mode selected_mode,
                               std::uint8_t* buffer,
                               std::size_t len,
                               rx_delivery delivery) noexcept
{
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr || selected_mode != mode::dma ||
        !configs[selected].has_rx_dma || !dma_accessible(buffer, len) ||
        len > std::numeric_limits<std::uint16_t>::max())
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
    ctx->rx_delivery = delivery;
    return types::status::ok;
}

types::status backend_restart_rx(port selected) noexcept
{
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr || ctx->rx_buffer == nullptr)
    {
        return types::status::invalid_arg;
    }
    const types::status status = start_dma_rx(
        selected, ctx->rx_buffer, ctx->rx_buffer_len, ctx->rx_delivery);
    if (status == types::status::ok)
    {
        ctx->rx_cursor = 0U;
    }
    return status;
}

} // namespace bsp::usart::detail

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* handle,
                                           std::uint16_t size)
{
    const bsp::usart::port selected = port_of(handle);
    backend_state* ctx = state_of(selected);
    if (ctx == nullptr || ctx->rx_buffer == nullptr ||
        size > ctx->rx_buffer_len)
    {
        return;
    }
    if (rx_dma_is_circular(handle))
    {
        const auto event_type = HAL_UARTEx_GetRxEventType(handle);
        if (ctx->rx_delivery == bsp::usart::rx_delivery::frame_snapshot)
        {
            if (event_type != HAL_UART_RXEVENT_IDLE || size == 0U)
            {
                return;
            }
            bsp::usart::detail::rx_from_isr(selected, 0U, size);
            return;
        }
        const auto event =
            event_type == HAL_UART_RXEVENT_TC
                ? bsp::usart::detail::rx_event::transfer_complete
                : event_type == HAL_UART_RXEVENT_HT
                      ? bsp::usart::detail::rx_event::half_transfer
                      : bsp::usart::detail::rx_event::idle;
        const auto update =
            bsp::usart::detail::circular_rx_segments(
                ctx->rx_cursor, size, ctx->rx_buffer_len, event);
        if (update.first.len != 0U)
        {
            bsp::usart::detail::rx_from_isr(
                selected, update.first.offset, update.first.len);
        }
        if (update.second.len != 0U)
        {
            bsp::usart::detail::rx_from_isr(
                selected, update.second.offset, update.second.len);
        }
        ctx->rx_cursor = update.next;
        return;
    }
    if (HAL_UARTEx_GetRxEventType(handle) == HAL_UART_RXEVENT_HT)
    {
        return;
    }
    bsp::usart::detail::rx_from_isr(selected, 0U, size);
    (void)bsp::usart::detail::backend_restart_rx(selected);
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* handle)
{
    const bsp::usart::port selected = port_of(handle);
    if (selected >= bsp::usart::port_count)
    {
        return;
    }
    __HAL_UART_CLEAR_OREFLAG(handle);
    if (tx_error_ended_transfer(handle, states[selected]))
    {
        (void)finish_tx_from_isr(states[selected], false);
    }
    bsp::usart::detail::error_from_isr(selected);
    if (states[selected].rx_buffer != nullptr)
    {
        (void)bsp::usart::detail::backend_restart_rx(selected);
    }
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* handle)
{
    const bsp::usart::port selected = port_of(handle);
    backend_state* ctx = state_of(selected);
    if (ctx != nullptr)
    {
        (void)finish_tx_from_isr(*ctx, true);
    }
}

extern "C" void
HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef* handle)
{
    const bsp::usart::port selected = port_of(handle);
    backend_state* ctx = state_of(selected);
    if (ctx != nullptr && finish_tx_from_isr(*ctx, false))
    {
        bsp::usart::detail::error_from_isr(selected);
    }
}

extern "C" void HAL_UART_AbortCpltCallback(
    UART_HandleTypeDef* handle)
{
    HAL_UART_AbortTransmitCpltCallback(handle);
}
