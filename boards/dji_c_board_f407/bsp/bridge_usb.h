#pragma once

/* 連接 USBX callback 與 board-neutral USB BSP 的內部 bridge；非應用程式 API。 */

#ifdef __cplusplus
extern "C" {
#endif

void usb_cdc_activate(void* cdc_acm_instance);
void usb_cdc_deactivate(void* cdc_acm_instance);
void usb_cdc_parameter_change(void* cdc_acm_instance);

#ifdef __cplusplus
}
#endif
