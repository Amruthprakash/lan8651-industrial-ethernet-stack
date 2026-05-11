
#include "tusb.h"
#include "tc6.h"

extern TC6_t *tc6;

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if (size < 14) { tud_network_recv_renew(); return true; }

    uint8_t txc, rxc; bool sync;
    TC6_GetState(tc6, &txc, &rxc, &sync);

    if (!sync || txc < 1) { tud_network_recv_renew(); return true; }

    // copy frame locally — src buffer belongs to TinyUSB and is freed
    // immediately after tud_network_recv_renew()
    static uint8_t tx_copy[1600];
    if (size <= sizeof(tx_copy))
        memcpy(tx_copy, src, size);

    tud_network_recv_renew();   // release TinyUSB buffer FIRST

    if (size <= sizeof(tx_copy))
        TC6_SendRawEthernetPacket(tc6, tx_copy, size, 0, NULL, NULL);

    return true;
}

/* TinyUSB calls this when it needs the frame data */
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    // ref = pointer passed to tud_network_xmit, arg = length
    memcpy(dst, (uint8_t *)ref, arg);
    return arg;
}
