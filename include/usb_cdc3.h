#ifndef USB_CDC3_H
#define USB_CDC3_H

#include <stdint.h>

#define USB_VID         0x1D50U
#define USB_PID  0x6019U

#define CDC_PORT_COUNT  3U
#define CDC_DATA_MPS    16U
#define CDC_NOTIF_MPS   16U
#define EP0_MPS         64U

void usb_cdc3_init(void);
void usb_cdc3_poll(void);
int  usb_cdc3_port_ready(uint8_t port);

#endif