#include "dbus_rx.hpp"

#include "config.hpp"
#include "msg.hpp"
#include "remoter.hpp"
#include "tx_api.h"

#include <cstdint>

namespace demo::cboard::dbus_rx
{

extern "C" {
volatile telemetry pnx_f407_dbus_telemetry{};
}

namespace
{

TX_THREAD monitor_thread{};
alignas(8) std::uint8_t monitor_stack[768]{};
msg::subscriber remote_sub{};

void monitor_entry(ULONG)
{
    remote_sub = msg::subscribe<remoter::dr16_state>();
    if (!remote_sub.valid())
    {
        pnx_f407_dbus_telemetry.init_failed = 1U;
        return;
    }

    remoter::dr16_state sample{};
    for (;;)
    {
        ++pnx_f407_dbus_telemetry.heartbeat;
        if (msg::read(remote_sub, sample) ==
            types::status::ok)
        {
            ++pnx_f407_dbus_telemetry.update_count;
            pnx_f407_dbus_telemetry.offline =
                sample.data.offline ? 1U : 0U;
            pnx_f407_dbus_telemetry.right_x =
                sample.data.right_x;
            pnx_f407_dbus_telemetry.right_y =
                sample.data.right_y;
            pnx_f407_dbus_telemetry.left_x =
                sample.data.left_x;
            pnx_f407_dbus_telemetry.left_y =
                sample.data.left_y;
        }
        tx_thread_sleep(10U);
    }
}

} // namespace

void run() noexcept
{
    remoter::dr16_config cfg{};
    cfg.uart_port = app::uart::usart3;
    cfg.thread_priority = 2U;
    cfg.rx_timeout_ticks = 100U;

    if (!remoter::dr16::instance().init(cfg))
    {
        pnx_f407_dbus_telemetry.init_failed = 1U;
        return;
    }
    if (tx_thread_create(
            &monitor_thread, const_cast<CHAR*>("dbus monitor"),
            monitor_entry, 0U, monitor_stack,
            sizeof(monitor_stack), 3U, 3U, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS)
    {
        pnx_f407_dbus_telemetry.init_failed = 1U;
    }
}

} // namespace demo::cboard::dbus_rx
