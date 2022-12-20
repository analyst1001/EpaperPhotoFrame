#include "stm32l4xx_hal.h"

/**
 * Handle SysTick for proper HAL operation
 */
void SysTick_Handler(void) {
	HAL_SYSTICK_IRQHandler();
	HAL_IncTick();
}
