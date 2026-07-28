#include "cboard/board_smoke/board_smoke.hpp"
#include "cboard/can_m2006/can_m2006.hpp"
#include "cboard/dbus_rx/dbus_rx.hpp"
#include "cboard/imu_bmi088/imu_bmi088.hpp"
#include "cboard/pwm_a2/pwm_a2.hpp"
#include "cboard/usb_cdc/usb_cdc.hpp"

#if (defined(PNX_APP_BOARD_SMOKE) + defined(PNX_APP_USB_CDC) + \
     defined(PNX_APP_CAN_M2006_VALIDATION) + \
     defined(PNX_APP_PWM_A2) + \
     defined(PNX_APP_BMI088) + defined(PNX_APP_DBUS_RX)) != 1
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
#elif defined(PNX_APP_BMI088)
    demo::cboard::imu_bmi088::run();
#elif defined(PNX_APP_DBUS_RX)
    demo::cboard::dbus_rx::run();
#elif defined(PNX_APP_CAN_M2006_VALIDATION)
    demo::cboard::can_m2006::run();
#endif
}
