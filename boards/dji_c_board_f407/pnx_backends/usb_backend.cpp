#include "bsp_usb_backend.hpp"

#include "bridge_usb.h"
#include "memory.h"
#include "tx_api.h"
#include "usb_tx_completion.hpp"
#include "usb_otg.h"
#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"

#include <atomic>
#include <cstdint>

#ifndef PNX_USB_DEVICE_IDENTITY_CONFIRMED
#define PNX_USB_DEVICE_IDENTITY_CONFIRMED 0
#endif

extern "C" UINT _ux_dcd_stm32_initialize(
    ULONG dcd_io, ULONG parameter);

namespace
{

constexpr std::uint16_t io_buffer_size =
    pnx::f407::usb_detail::cdc_full_speed_max_packet_size;
constexpr std::uint16_t worker_stack_size = 1536U;
constexpr ULONG rx_fifo_words = 128U;
constexpr ULONG ep0_tx_fifo_words = 16U;
constexpr ULONG cdc_command_tx_fifo_words = 16U;
constexpr ULONG cdc_data_tx_fifo_words = 32U;
constexpr std::uint32_t connection_event_none = 0U;
constexpr std::uint32_t connection_event_disconnected = 1U;
constexpr std::uint32_t connection_event_connected = 2U;

alignas(8) std::uint8_t worker_stack[worker_stack_size]{};
alignas(4) std::uint8_t tx_buffer[io_buffer_size]{};

TX_THREAD worker_thread{};
TX_SEMAPHORE write_signal{};
TX_MUTEX common_mutex{};

bsp::usb::detail::backend_config active_config{};
UX_SLAVE_CLASS_CDC_ACM* active_cdc = nullptr;
pnx::f407::usb_detail::tx_completion_state active_tx{};
bool mutex_ready = false;
bool signal_ready = false;
bool controller_started = false;
std::atomic<std::uint32_t> pending_connection_event{
    connection_event_none};

static_assert(
    std::atomic<std::uint32_t>::is_always_lock_free,
    "F407 USB ISR-to-thread connection event must be lock-free");

bool device_ready() noexcept
{
    return active_cdc != nullptr && controller_started &&
           _ux_system_slave != nullptr &&
           _ux_system_slave->ux_system_slave_device.ux_slave_device_state ==
               UX_DEVICE_CONFIGURED;
}

void publish_native_error(UINT native_status) noexcept
{
    bsp::usb::detail::receive_from_backend(
        nullptr, 0U, types::status::error, native_status);
}

UINT cdc_write_complete(UX_SLAVE_CLASS_CDC_ACM* cdc, UINT status,
                        ULONG length)
{
    auto completion = active_tx.on_callback(
        status == UX_SUCCESS, static_cast<std::uint16_t>(length));
    if (completion.next ==
        pnx::f407::usb_detail::tx_next_action::send_zlp)
    {
        const UINT zlp_status =
            ux_device_class_cdc_acm_write_with_callback(
                cdc, tx_buffer, 0U);
        if (zlp_status == UX_SUCCESS)
        {
            return UX_SUCCESS;
        }
        completion = active_tx.abort();
        status = zlp_status;
    }

    bsp::usb::detail::transmit_complete_from_backend(
        completion.requested, completion.actual,
        completion.success ? types::status::ok : types::status::error,
        status);
    bsp::usb::detail::backend_signal_tx();
    return UX_SUCCESS;
}

UINT cdc_read_complete(UX_SLAVE_CLASS_CDC_ACM*, UINT status,
                       UCHAR* data, ULONG length)
{
    bsp::usb::detail::receive_from_backend(
        data, static_cast<std::uint16_t>(length),
        status == UX_SUCCESS ? types::status::ok : types::status::error,
        status);
    return UX_SUCCESS;
}

UINT start_controller() noexcept
{
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, rx_fifo_words);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, ep0_tx_fifo_words);
    HAL_PCDEx_SetTxFiFo(
        &hpcd_USB_OTG_FS, 2U, cdc_command_tx_fifo_words);
    HAL_PCDEx_SetTxFiFo(
        &hpcd_USB_OTG_FS, 3U, cdc_data_tx_fifo_words);

    UINT status = _ux_dcd_stm32_initialize(
        reinterpret_cast<ULONG>(USB_OTG_FS),
        reinterpret_cast<ULONG>(&hpcd_USB_OTG_FS));
    if (status != UX_SUCCESS)
    {
        return status;
    }
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK)
    {
        return UX_ERROR;
    }
    controller_started = true;
    return UX_SUCCESS;
}

void worker_entry(ULONG)
{
    const UINT controller_status = start_controller();
    if (controller_status != UX_SUCCESS)
    {
        publish_native_error(controller_status);
    }

    for (;;)
    {
        if (tx_semaphore_get(&write_signal, TX_WAIT_FOREVER) !=
            TX_SUCCESS)
        {
            publish_native_error(UX_ERROR);
            continue;
        }

        const std::uint32_t connection_event =
            pending_connection_event.exchange(
                connection_event_none, std::memory_order_acq_rel);
        if (connection_event != connection_event_none)
        {
            bsp::usb::detail::connection_from_backend(
                connection_event == connection_event_connected);
        }

        bsp::usb::detail::transmit_woken_from_backend();
        if (!device_ready() || active_tx.busy())
        {
            continue;
        }

        std::uint16_t requested = 0U;
        if (!bsp::usb::detail::prepare_tx_for_backend(
                tx_buffer, sizeof(tx_buffer), requested))
        {
            continue;
        }

        active_tx.start(requested);
        const UINT status =
            ux_device_class_cdc_acm_write_with_callback(
                active_cdc, tx_buffer, requested);
        if (status != UX_SUCCESS)
        {
            const auto completion = active_tx.abort();
            bsp::usb::detail::transmit_complete_from_backend(
                completion.requested, completion.actual,
                types::status::error, status);
        }
    }
}

} // namespace

extern "C" void usb_cdc_activate(void* cdc_acm_instance)
{
    auto* cdc = static_cast<UX_SLAVE_CLASS_CDC_ACM*>(cdc_acm_instance);
    UX_SLAVE_CLASS_CDC_ACM_CALLBACK_PARAMETER callbacks{};
    callbacks.ux_device_class_cdc_acm_parameter_write_callback =
        cdc_write_complete;
    callbacks.ux_device_class_cdc_acm_parameter_read_callback =
        cdc_read_complete;
    const UINT status = ux_device_class_cdc_acm_ioctl(
        cdc, UX_SLAVE_CLASS_CDC_ACM_IOCTL_TRANSMISSION_START,
        &callbacks);
    if (status != UX_SUCCESS)
    {
        publish_native_error(status);
        return;
    }
    active_cdc = cdc;
    pending_connection_event.store(
        connection_event_connected, std::memory_order_release);
    bsp::usb::detail::backend_signal_tx();
}

extern "C" void usb_cdc_deactivate(void* cdc_acm_instance)
{
    auto* cdc = static_cast<UX_SLAVE_CLASS_CDC_ACM*>(cdc_acm_instance);
    if (cdc != nullptr)
    {
        (void)ux_device_class_cdc_acm_ioctl(
            cdc, UX_SLAVE_CLASS_CDC_ACM_IOCTL_TRANSMISSION_STOP,
            UX_NULL);
    }
    active_cdc = nullptr;
    active_tx.reset();
    pending_connection_event.store(
        connection_event_disconnected, std::memory_order_release);
    bsp::usb::detail::backend_signal_tx();
}

extern "C" void usb_cdc_parameter_change(void* cdc_acm_instance)
{
    UX_PARAMETER_NOT_USED(cdc_acm_instance);
}

namespace bsp::usb::detail
{

types::status backend_init(const backend_config& config) noexcept
{
    if (PNX_USB_DEVICE_IDENTITY_CONFIRMED == 0)
    {
        (void)config;
        return types::status::not_configured;
    }

    active_config = config;
    if (!mutex_ready)
    {
        if (tx_mutex_create(
                &common_mutex, const_cast<CHAR*>("usb_common"),
                TX_INHERIT) != TX_SUCCESS)
        {
            return types::status::error;
        }
        mutex_ready = true;
    }
    if (!signal_ready)
    {
        if (tx_semaphore_create(
                &write_signal, const_cast<CHAR*>("usb_tx_ready"),
                0U) != TX_SUCCESS)
        {
            return types::status::error;
        }
        signal_ready = true;
    }
    if (tx_thread_create(
            &worker_thread, const_cast<CHAR*>("usb_f407_tx"),
            worker_entry, 0U, worker_stack, sizeof(worker_stack),
            active_config.write_priority,
            active_config.write_priority, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS)
    {
        return types::status::error;
    }
    return types::status::ok;
}

bool backend_connected() noexcept
{
    return device_ready();
}

void backend_signal_tx() noexcept
{
    if (signal_ready)
    {
        (void)tx_semaphore_put(&write_signal);
    }
}

capabilities backend_capabilities() noexcept
{
    capabilities result{};
    result.device_mode = true;
    result.high_speed = false;
    result.supports_vbus_sense = false;
    result.max_packet_size = io_buffer_size;
    result.max_transfer_size = io_buffer_size;
    return result;
}

bool backend_lock() noexcept
{
    return mutex_ready &&
           tx_mutex_get(&common_mutex, TX_WAIT_FOREVER) == TX_SUCCESS;
}

void backend_unlock() noexcept
{
    if (mutex_ready)
    {
        (void)tx_mutex_put(&common_mutex);
    }
}

} // namespace bsp::usb::detail
