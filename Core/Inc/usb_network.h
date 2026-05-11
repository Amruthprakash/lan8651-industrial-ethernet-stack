#ifndef USB_NETWORK_H
#define USB_NETWORK_H

#include "netif.h"

void usb_network_init(void);
struct netif* usb_network_netif(void);

#endif
