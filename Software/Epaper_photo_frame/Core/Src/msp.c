#include "stm32l4xx_hal.h"

extern void SDC_SPI_Msp_Init(void);
extern void EPD_SPI_Msp_Init(void);
extern void Logger_Msp_Init(void);
extern void RTC_Msp_Init(void);
extern void EPD_SPI_Msp_Deinit(void);
extern void SDC_SPI_Msp_De_Init(void);

/**
 * Low level initialization for STM32 HAL
 */
void HAL_MspInit(void) {
    // Enable USAGEFAULT, BUSFAULT, and MEMFAULT
    SCB->SHCSR = 0x7 << 16;

    // Configure priority for system exceptions
    HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
    HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
    HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
}


/**
 * Low level initialization for SPI peripherals
 *
 * @param hspi  (IN)    Handle to SPI peripheral
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        EPD_SPI_Msp_Init();
    }
    else if (hspi->Instance == SPI2) {
        SDC_SPI_Msp_Init();
    }
}

/**
 * Low level initialization for RTC peripheral
 * @param hrtc  (UNUSED)    Handle to RTC peripheral
 */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc) {
    RTC_Msp_Init();
}

/**
 * Low level de-initialization for SPI peripherals
 *
 * @param hspi  (IN)    Handle to SPI peripheral
 */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        EPD_SPI_Msp_Deinit();
    }
    else if (hspi->Instance == SPI2) {
        SDC_SPI_Msp_De_Init();
    }
}
