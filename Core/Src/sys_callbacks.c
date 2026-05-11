#include "tc6.h"
#include "stm32h5xx_hal.h"
#include "spi.h"
#include "tusb.h"
#include <string.h>
#include <stdbool.h>

#define RX_BUF_SIZE 1600

uint8_t rxFrame[RX_BUF_SIZE];
static uint8_t rxBuf[2][RX_BUF_SIZE];
static uint16_t rxLen[2];
static uint8_t writeIdx = 0;   // TC6 writes here
static uint8_t readIdx  = 0;   // USB reads here
/* ============================================================
   SPI CALLBACK  (MOST IMPORTANT)
============================================================ */

bool TC6_CB_OnSpiTransaction(
        uint8_t inst,
        uint8_t *tx,
        uint8_t *rx,
        uint16_t len,
        void *tag)
{
    HAL_GPIO_WritePin(GPIOH,GPIO_PIN_4,GPIO_PIN_RESET);

    HAL_SPI_TransmitReceive(
            &hspi2,
            tx,
            rx,
            len,
            HAL_MAX_DELAY);



    HAL_GPIO_WritePin(GPIOH,GPIO_PIN_4,GPIO_PIN_SET);

    TC6_SpiBufferDone(inst,true);

    return true;
}

/* ============================================================
   REQUIRED EMPTY CALLBACK
============================================================ */

void TC6_CB_OnNeedService(TC6_t *g, void *tag)
{
}

/* ============================================================
   RX SLICE (frame assembly)
============================================================ */

void TC6_CB_OnRxEthernetSlice(
        TC6_t *g,
        const uint8_t *data,
        uint16_t offset,
        uint16_t len,
        void *tag)
{
    if (offset + len <= RX_BUF_SIZE)
        memcpy(&rxBuf[writeIdx][offset], data, len);
}

/* ============================================================
   RX COMPLETE
============================================================ */

void TC6_CB_OnRxEthernetPacket(
        TC6_t *g,
        bool success,
        uint16_t length,
        uint64_t *timestamp,
        void *tag)
{
    if (!success) return;

    uint8_t idx = writeIdx;
    writeIdx ^= 1;              // flip write buffer immediately

    // retry up to 5ms if USB is briefly busy
    for (int i = 0; i < 5; i++) {
        if (tud_network_can_xmit(length)) {
            tud_network_xmit(rxBuf[idx], length);
            return;
        }
        HAL_Delay(1);
        tud_task();
    }
}
/* ============================================================
   ERROR CALLBACK
============================================================ */

void TC6_CB_OnError(
        TC6_t *g,
        TC6_Error_t err,
        void *tag)
{
   ;
}
