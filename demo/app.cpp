#include "cboard/board_smoke/board_smoke.hpp"
#include "cboard/pwm_a2/pwm_a2.hpp"
#include "cboard/usb_cdc/usb_cdc.hpp"

// 所有 F407 韌體共用的應用分派點。
// CMake 透過 PNX_APP_* 編譯定義決定本次 ELF 要執行哪一個 closure；
// MyCar 底盤映像會沿著 app_start() -> vehicle::mycar::run() -> chassis runtime 啟動。

#if defined(PNX_APP_MYCAR_CHASSIS)
#include "vehicle/mycar.hpp"
#endif

#if (defined(PNX_APP_BOARD_SMOKE) + defined(PNX_APP_USB_CDC) + \
     defined(PNX_APP_PWM_A2) + defined(PNX_APP_MYCAR_CHASSIS)) != 1
#error "Exactly one F407 application must be selected"
#endif

extern "C" void app_start()
{
    // ThreadX 開始排程前呼叫的 C ABI 入口。
    // 每個 closure 要自行建立後續 thread，或只執行一次板級 smoke test。
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
