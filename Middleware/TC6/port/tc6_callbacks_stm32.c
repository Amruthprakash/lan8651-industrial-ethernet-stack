#include "tc6.h"
#include "main.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi2;
//
/* =====================================================
   SPI TRANSACTION
=====================================================*/



/* =====================================================
   EXTENDED STATUS
=====================================================*/


/* =====================================================
   REG DRIVER CALLBACKS
=====================================================*/
uint32_t TC6Regs_CB_GetTicksMs(void)
{
    return HAL_GetTick();
}

void TC6Regs_CB_OnEvent(uint32_t evt)
{
}
