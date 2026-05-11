/*
 * board_port.c
 *
 *  Created on: Mar 5, 2026
 *      Author: LENOVO
 */


#include "stm32h5xx.h"

void board_get_unique_id(uint8_t id[16])
{
    uint32_t *uid = (uint32_t*)UID_BASE;

    for(int i=0;i<3;i++)
    {
        id[i*4+0] = (uid[i] >> 0) & 0xFF;
        id[i*4+1] = (uid[i] >> 8) & 0xFF;
        id[i*4+2] = (uid[i] >> 16) & 0xFF;
        id[i*4+3] = (uid[i] >> 24) & 0xFF;
    }
}




