#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "stm32l4xx_hal.h"
#include "logging.h"
#include "main.h"

#ifdef LOG_ENABLED

// Use UART4 TX - PA0 GPIO
static UART_HandleTypeDef huart4;


void Logger_Init(void) {
	huart4.Instance = UART4;
	huart4.Init.BaudRate = 115200;
	huart4.Init.WordLength = UART_WORDLENGTH_8B;
	huart4.Init.StopBits = UART_STOPBITS_1;
	huart4.Init.Parity = UART_PARITY_NONE;
	huart4.Init.Mode = UART_MODE_TX;
	huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart4.Init.OverSampling = UART_OVERSAMPLING_16;
	huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;

	if (HAL_UART_Init(&huart4) != HAL_OK) {
		Error_Handler();
	}
}

void Log_Msg(const char *restrict const msg, ...) {
	static char formatted_msg[MAX_LOG_MSG_SIZE];

	va_list args;
	va_start(args, msg);
	vsnprintf(formatted_msg, MAX_LOG_MSG_SIZE, msg, args);
	va_end(args);

	if (HAL_UART_Transmit(&huart4, (uint8_t *)formatted_msg, strlen(formatted_msg),
			HAL_MAX_DELAY) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * Log level initialization for UART4 peripheral
 */
void Logger_Msp_Init(void) {
	// UART4 uses PA0(TX) for transmitting data - Table 18 in MCU datasheet
	GPIO_InitTypeDef gpio_uart = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_UART4_CLK_ENABLE();

	gpio_uart.Pin = GPIO_PIN_0;
	gpio_uart.Mode = GPIO_MODE_AF_PP;
	gpio_uart.Pull = GPIO_NOPULL;
	gpio_uart.Speed = GPIO_SPEED_FREQ_LOW;
	gpio_uart.Alternate = GPIO_AF8_UART4;

	HAL_GPIO_Init(GPIOA, &gpio_uart);
}

#else

void Logger_Init(void) {};

void Log_Msg(const char *restrict const msg, ...) {};

#endif
