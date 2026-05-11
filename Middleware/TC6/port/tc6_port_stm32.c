#include "tc6_port_stm32.h"
#include "main.h"

extern SPI_HandleTypeDef hspi2;

/* ================= CS CONTROL ================= */

static inline void tc6_cs_low(void)
{
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        GPIO_PIN_RESET);
}

static inline void tc6_cs_high(void)
{
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        GPIO_PIN_SET);
}

/* ================= SPI ================= */

void tc6_spi_transfer(uint8_t *tx,
                      uint8_t *rx,
                      uint16_t len)
{
    tc6_cs_low();

    HAL_SPI_TransmitReceive(
        &hspi2,
        tx,
        rx,
        len,
        HAL_MAX_DELAY);

    tc6_cs_high();
}

/* ================= DELAY ================= */

void tc6_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks =
        us * (SystemCoreClock / 1000000);

    while ((DWT->CYCCNT - start) < ticks);
}

void tc6_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

void tc6_lock(void){}
void tc6_unlock(void){}
