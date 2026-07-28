#include "board_smoke.hpp"

#include "bsp_dwt.hpp"
#include "bsp_indicator.hpp"
#include "tx_api.h"

namespace demo::cboard::board_smoke
{
namespace
{

TX_THREAD core_thread{};
alignas(8) ULONG core_stack[256]{};
CHAR core_name[] = "f407 core";

void core_entry(ULONG)
{
    const bool initialized =
        bsp::dwt::init() == types::status::ok &&
        bsp::indicator::init() == types::status::ok;
    if (!initialized)
    {
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
    }

    for (;;)
    {
        if (initialized)
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
            &core_thread, core_name, core_entry, 0U,
            core_stack, sizeof(core_stack), 10U, 10U,
            TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        (void)bsp::indicator::init();
        (void)bsp::indicator::set(
            bsp::indicator::channel::red, true);
    }
}

} // namespace demo::cboard::board_smoke
