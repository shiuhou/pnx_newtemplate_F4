#include "cboard/board_smoke/board_smoke.hpp"
#include "cboard/usb_cdc/usb_cdc.hpp"

#if defined(PNX_APP_BOARD_SMOKE) && defined(PNX_APP_USB_CDC)
#error "Only one F407 application may be selected"
#elif !defined(PNX_APP_BOARD_SMOKE) && !defined(PNX_APP_USB_CDC)
#error "An F407 application must be selected"
#endif

extern "C" void app_start()
{
#if defined(PNX_APP_BOARD_SMOKE)
    demo::cboard::board_smoke::run();
#else
    demo::cboard::usb_cdc::run();
#endif
}
