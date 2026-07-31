#include "cboard/board_smoke/board_smoke.hpp"
#include "cboard/pwm_a2/pwm_a2.hpp"
#include "cboard/usb_cdc/usb_cdc.hpp"

#if defined(PNX_APP_MYCAR_CHASSIS)
#include "vehicle/mycar.hpp"
#endif

#if (defined(PNX_APP_BOARD_SMOKE) + defined(PNX_APP_USB_CDC) + \
     defined(PNX_APP_PWM_A2) + defined(PNX_APP_MYCAR_CHASSIS)) != 1
#error "Exactly one F407 application must be selected"
#endif

extern "C" void app_start()
{
#if defined(PNX_APP_BOARD_SMOKE)
    demo::cboard::board_smoke::run();
#elif defined(PNX_APP_USB_CDC)
    demo::cboard::usb_cdc::run();
#elif defined(PNX_APP_PWM_A2)
    demo::cboard::pwm_a2::run();
#elif defined(PNX_APP_MYCAR_CHASSIS)
    vehicle::mycar::run();
#endif
}
