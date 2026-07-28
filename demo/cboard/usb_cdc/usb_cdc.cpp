#include "usb_cdc.hpp"

#include "bsp_indicator.hpp"
#include "bsp_usb.hpp"
#include "tx_api.h"

#include <cstdint>

namespace demo::cboard::usb_cdc
{
namespace
{

TX_THREAD usb_thread{};
alignas(8) ULONG usb_stack[256]{};
CHAR usb_name[] = "f407 usb cdc";

void echo_received(
    const std::uint8_t* data, std::uint16_t length, void*) noexcept
{
    if (data != nullptr && length != 0U)
    {
        (void)bsp::usb::write(data, length);
    }
}

void usb_entry(ULONG)
{
    const bool indicator_ready =
        bsp::indicator::init() == types::status::ok;
    bsp::usb::config config{};
    config.on_rx = echo_received;
    const types::status usb_status = bsp::usb::init(config);

    if (!indicator_ready ||
        (usb_status != types::status::ok &&
         usb_status != types::status::not_configured))
    {
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
    }

    for (;;)
    {
        if (indicator_ready)
        {
            (void)bsp::indicator::toggle(
                bsp::indicator::channel::green);
        }
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2U);
    }
}

} // namespace

void run() noexcept
{
    if (tx_thread_create(
            &usb_thread, usb_name, usb_entry, 0U,
            usb_stack, sizeof(usb_stack), 10U, 10U,
            TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        (void)bsp::indicator::init();
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
    }
}

} // namespace demo::cboard::usb_cdc
