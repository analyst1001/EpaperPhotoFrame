#include "stm32l4xx_hal.h"
#include "led.h"

void Busy_LED_Init(void) {
    // Use PB6 for LED indicating busy status for the board - SD card/EPD work
    // starts
    GPIO_InitTypeDef busy_led_gpio = { 0 };

    __HAL_RCC_GPIOB_CLK_ENABLE();

    busy_led_gpio.Pin = GPIO_PIN_6;
    busy_led_gpio.Mode = GPIO_MODE_OUTPUT_PP;
    busy_led_gpio.Pull = GPIO_NOPULL;
    busy_led_gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &busy_led_gpio);
}

void Busy_LED_Indicate_Work_Start(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, SET);
}

void Busy_LED_Indicate_Work_End(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, RESET);
}

void Busy_LED_De_Init(void) {
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);
}

void Error_LED_Init(void) {
    // Use PB5 for LED indicating that a system error has occurred
    GPIO_InitTypeDef error_led_gpio = { 0 };

    __HAL_RCC_GPIOB_CLK_ENABLE();

    error_led_gpio.Pin = GPIO_PIN_5;
    error_led_gpio.Mode = GPIO_MODE_OUTPUT_PP;
    error_led_gpio.Pull = GPIO_NOPULL;
    error_led_gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &error_led_gpio);
}

void Error_LED_Indicate_Error(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, SET);
}

void Error_LED_De_Init(void) {
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_5);
}
