#include "bsp_can.hpp"

#include "can.h"
#include "stm32f4xx_hal.h"

namespace
{

CAN_HandleTypeDef* handle_from_id(bsp::can::handle_id id) noexcept
{
    switch (id)
    {
    case bsp::can::handle_id::can1:
        return &hcan1;
    case bsp::can::handle_id::can2:
        return &hcan2;
    default:
        return nullptr;
    }
}

CAN_HandleTypeDef* handle_of(bsp::can::bus selected) noexcept
{
    const auto index = static_cast<std::size_t>(selected);
    if (index >= bsp::can::bus_count || !bsp::can::configs[index].enabled)
    {
        return nullptr;
    }
    return handle_from_id(bsp::can::configs[index].handle);
}

bsp::can::bus bus_of(CAN_HandleTypeDef* handle) noexcept
{
    for (std::size_t index = 0; index < bsp::can::bus_count; ++index)
    {
        const auto selected = static_cast<bsp::can::bus>(index);
        if (handle_of(selected) == handle)
        {
            return selected;
        }
    }
    return bsp::can::bus::none;
}

constexpr std::uint32_t can_notifications =
    CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN |
    CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF |
    CAN_IT_LAST_ERROR_CODE | CAN_IT_ERROR;

} // namespace

namespace bsp::can::detail
{

types::status backend_init(bus selected, bus_type type) noexcept
{
    CAN_HandleTypeDef* handle = handle_of(selected);
    if (handle == nullptr || type != bus_type::classic)
    {
        return types::status::invalid_arg;
    }

    CAN_FilterTypeDef filter{};
    filter.FilterBank = handle->Instance == CAN1 ? 0U : 14U;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0U;
    filter.FilterIdLow = CAN_ID_STD | CAN_RTR_DATA;
    filter.FilterMaskIdHigh = 0U;
    filter.FilterMaskIdLow = CAN_ID_EXT | CAN_RTR_REMOTE;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14U;

    if (HAL_CAN_ConfigFilter(handle, &filter) != HAL_OK ||
        HAL_CAN_Start(handle) != HAL_OK)
    {
        return types::status::error;
    }

    return HAL_CAN_ActivateNotification(handle, can_notifications) == HAL_OK
               ? types::status::ok
               : types::status::error;
}

types::status backend_transmit(bus selected, std::uint32_t id,
                               const std::uint8_t* data,
                               std::uint16_t len) noexcept
{
    CAN_HandleTypeDef* handle = handle_of(selected);
    if (handle == nullptr || data == nullptr || len > 8U)
    {
        return types::status::invalid_arg;
    }

    CAN_TxHeaderTypeDef header{};
    if (id > 0x7FFU)
    {
        header.ExtId = id;
        header.IDE = CAN_ID_EXT;
    }
    else
    {
        header.StdId = id;
        header.IDE = CAN_ID_STD;
    }
    header.RTR = CAN_RTR_DATA;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;

    std::uint32_t mailbox = 0U;
    return HAL_CAN_AddTxMessage(handle, &header,
                                const_cast<std::uint8_t*>(data),
                                &mailbox) == HAL_OK
               ? types::status::ok
               : types::status::busy;
}

types::status backend_recover(bus selected) noexcept
{
    CAN_HandleTypeDef* handle = handle_of(selected);
    if (handle == nullptr)
    {
        return types::status::invalid_arg;
    }
    if (HAL_CAN_Stop(handle) != HAL_OK ||
        HAL_CAN_Start(handle) != HAL_OK ||
        HAL_CAN_ActivateNotification(
            handle, can_notifications) != HAL_OK)
    {
        return types::status::error;
    }
    return types::status::ok;
}

std::uint32_t backend_tick_now() noexcept
{
    return HAL_GetTick();
}

std::uint32_t backend_enter_critical() noexcept
{
    const std::uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

void backend_exit_critical(std::uint32_t state) noexcept
{
    if (state == 0U)
    {
        __enable_irq();
    }
}

} // namespace bsp::can::detail

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* handle)
{
    const bsp::can::bus selected = bus_of(handle);
    if (selected == bsp::can::bus::none)
    {
        return;
    }

    while (HAL_CAN_GetRxFifoFillLevel(handle, CAN_RX_FIFO0) != 0U)
    {
        CAN_RxHeaderTypeDef header{};
        bsp::can::rx_frame frame{};
        if (HAL_CAN_GetRxMessage(handle, CAN_RX_FIFO0, &header, frame.data) != HAL_OK)
        {
            bsp::can::detail::error_from_isr(
                selected, bsp::can::state::fault, HAL_GetTick());
            return;
        }
        if (header.IDE != CAN_ID_STD || header.RTR != CAN_RTR_DATA)
        {
            bsp::can::detail::drop_from_isr(selected);
            continue;
        }
        frame.id = header.IDE == CAN_ID_EXT ? header.ExtId : header.StdId;
        frame.len = static_cast<std::uint8_t>(header.DLC);
        bsp::can::detail::rx_from_isr(selected, frame, HAL_GetTick());
    }
}

extern "C" void HAL_CAN_ErrorCallback(CAN_HandleTypeDef* handle)
{
    const bsp::can::bus selected = bus_of(handle);
    if (selected == bsp::can::bus::none)
    {
        return;
    }

    const std::uint32_t error = HAL_CAN_GetError(handle);
    if ((error & HAL_CAN_ERROR_RX_FOV0) != 0U)
    {
        bsp::can::detail::drop_from_isr(selected);
    }
    bsp::can::state next_state = bsp::can::state::fault;
    if ((error & HAL_CAN_ERROR_BOF) != 0U)
    {
        next_state = bsp::can::state::bus_off;
    }
    else if ((error & HAL_CAN_ERROR_EPV) != 0U)
    {
        next_state = bsp::can::state::passive;
    }
    else if ((error & HAL_CAN_ERROR_EWG) != 0U)
    {
        next_state = bsp::can::state::warning;
    }
    bsp::can::detail::error_from_isr(selected, next_state, HAL_GetTick());
}
