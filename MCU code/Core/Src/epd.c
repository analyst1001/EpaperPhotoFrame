#include "stm32l4xx_hal.h"
#include "epd.h"

/*
 * Use SPI1 for communication with E-paper display: PA2(NSS), PA5(SCK),
 * and PA7(MOSI)
 *
 * D/C - PA1
 * RST - PA4
 * BUSY - PA15
 */
static SPI_HandleTypeDef hspi1;

/*
 * E-paper display commands - Section 6-1 in datasheet
 */
#define EPD_COMMAND_POWER_OFF		(0x02)
#define EPD_DATA_POWER_OFF			(0x00)
#define EPD_COMMAND_POWER_ON		(0x04)
#define EPD_COMMAND_DEEPSLEEP		(0x07)
#define EPD_DATA_DEEPSLEEP			(0xA5)
#define EPD_COMMAND_DATA_TX_START	(0x10)
#define EPD_COMMAND_DATA_REFRESH	(0x12)
#define EPD_DATA_DATA_REFRESH		(0x00)

/*
 * Some helpful macros
 */
#define EPD_DC_COMMAND()	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET)
#define EPD_DC_DATA()		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET)

#define EPD_RESET_LOW()		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define EPD_RESET_HIGH()	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

#define EPD_BUSY_READ()		HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)


#define EPD_CS_SELECT()		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET)
#define EPD_CS_DESELECT()	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET)

static HAL_StatusTypeDef SPI1_Init(void);
static EPD_Status EPD_Init_internal(void);
static void EPD_GPIOs_Init(void);
static void EPD_GPIOs_De_Init(void);
static void EPD_Reset(void);
static void EPD_Wait_While_Busy(void);
static HAL_StatusTypeDef EPD_Send_Command(uint8_t cmd);
static HAL_StatusTypeDef EPD_Send_Data(uint8_t data);
static EPD_Status EPD_Refresh_Display_Image(void);
static EPD_Status EPD_Send_Command_And_Data(
		const uint8_t cmd,
		const uint8_t *restrict const data,
		const uint8_t data_size);

EPD_Status EPD_Init(void) {
	if (SPI1_Init() != HAL_OK) {
		return EPD_INIT_IO_INIT_ERR;
	}

	EPD_GPIOs_Init();
	return EPD_Init_internal();
}

EPD_Status EPD_Display_Clear(const EPD_Color_t color) {
	const uint8_t display_color = (color << 4) | color;

	if (EPD_Send_Command(EPD_COMMAND_DATA_TX_START) != HAL_OK) {
		return EPD_SEND_CMD_ERR;
	}

	for (uint16_t i = 0; i < EPD_HEIGHT; i++) {
		for (uint16_t j = 0; j < EPD_WIDTH; j++) {
			if (EPD_Send_Data(display_color) != HAL_OK) {
				return EPD_SEND_DATA_ERR;
			}
		}
	}

	return EPD_Refresh_Display_Image();
}

EPD_Status EPD_Display_Full_Image(const uint8_t *restrict const img) {
	if (EPD_Send_Command(EPD_COMMAND_DATA_TX_START) != HAL_OK) {
		return EPD_SEND_CMD_ERR;
	}

	for (uint16_t i = 0; i < EPD_HEIGHT; i++) {
		for (uint16_t j = 0; j < EPD_WIDTH; j++) {
			if (EPD_Send_Data(img[i * EPD_WIDTH + j]) != HAL_OK) {
				return EPD_SEND_DATA_ERR;
			}
		}
	}

	return EPD_Refresh_Display_Image();
}

EPD_Status EPD_Put_To_Sleep(void) {
	uint8_t data = 0xA5;

	if (EPD_Send_Command_And_Data(0x07, &data, 1) != EPD_OK) {
		return EPD_SLEEP_DATA_SEND_ERR;
	}
	HAL_Delay(10);
	EPD_RESET_LOW();
	return EPD_OK;
}

DataProcessingStatus EPD_Display_Image_Callback(
		const uint32_t data_offset,
		const uint8_t *restrict const image_buffer,
		const uint32_t buffer_size) {
	uint32_t i;

	// If we receive the partial image data which represent start of full image
	// data, start TX
	if (data_offset == 0) {
		if (EPD_Send_Command(EPD_COMMAND_DATA_TX_START) != HAL_OK) {
			return DATA_PROCESSING_PROCESS_ERR;
		}
	}

	for (i = 0; i < buffer_size; i++) {
		if (EPD_Send_Data(image_buffer[i]) != HAL_OK) {
			return DATA_PROCESSING_PROCESS_ERR;
		}
		if ((data_offset + i) >= (EPD_WIDTH * EPD_HEIGHT)) {
			return DATA_PROCESSING_MORE_THAN_EXPECTED_DATA;
		}
	}

	// When all the data has been sent (in single call or multiple calls),
	// refresh the display to update the image on E-paper display
	if ((data_offset + i) == (EPD_WIDTH * EPD_HEIGHT)) {
		if (EPD_Refresh_Display_Image() != EPD_OK) {
			return DATA_PROCESSING_PROCESS_ERR;
		}
	}

	return DATA_PROCESSING_OK;
}

EPD_Status EPD_De_Init(void) {
//	if (HAL_SPI_DeInit(&hspi1) != HAL_OK) {
//		return EPD_DEINIT_IO_DEINIT_ERR;
//	}
//
//	EPD_GPIOs_De_Init();
	__HAL_RCC_SPI1_CLK_DISABLE();
	return EPD_OK;
}

/**
 * Initialize SPI1 for communication data with the
 * E-paper display
 */
static HAL_StatusTypeDef SPI1_Init(void) {
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	// SPI will receive fPCLK/4 = 20MHz. Use prescaler 8 to use 2.5MHz clock for
	// communication with E-paper display
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

	return HAL_SPI_Init(&hspi1);
}

/**
 * Low-level SPI1 peripheral initialization
 */
void EPD_SPI_Msp_Init(void) {
	// Use PA2(NSS), PA5(SCK), and PA7(MOSI) - Table 16 in MCU datasheet
	GPIO_InitTypeDef gpio_spi = {0};

	__HAL_RCC_SPI1_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	// Initialize the chip select GPIO
	gpio_spi.Pin = GPIO_PIN_2;
	gpio_spi.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_spi.Pull = GPIO_NOPULL;
	gpio_spi.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &gpio_spi);

	// Initialize the MOSI and SCK GPIOs
	gpio_spi.Pin = GPIO_PIN_5 | GPIO_PIN_7;
	gpio_spi.Mode = GPIO_MODE_AF_PP;
	gpio_spi.Pull = GPIO_NOPULL;
	gpio_spi.Speed = GPIO_SPEED_FREQ_HIGH;
	gpio_spi.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &gpio_spi);

	// De-select the chip as soon as GPIO initialization
	// is done
	EPD_CS_DESELECT();
}

/**
 * Low-level SPI1 peripheral de-initialization for IO channels
 */
void EPD_SPI_Msp_Deinit(void) {
	__HAL_RCC_SPI1_CLK_DISABLE();
	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_7);
}

/**
 * Initialize GPIOs for control signals to E-paper display
 */
static void EPD_GPIOs_Init(void) {
	// Use  PA1 for D/C, PA4 for RST, and PA15 for BUSY
	GPIO_InitTypeDef gpio_epd = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();

	gpio_epd.Pin = (GPIO_PIN_1 | GPIO_PIN_4);
	gpio_epd.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_epd.Pull = GPIO_NOPULL;
	gpio_epd.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &gpio_epd);

	gpio_epd.Pin = GPIO_PIN_15;
	gpio_epd.Mode = GPIO_MODE_INPUT;
	HAL_GPIO_Init(GPIOD, &gpio_epd);
}

/**
 * Low-level de-initialization of GPIOs used as control signals for E-paper
 * display
 */
static void EPD_GPIOs_De_Init(void) {
	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4);
}

/**
 * Initialize the E-paper display. Steps taken from
 * https://github.com/waveshare/e-Paper/blob/master/STM32/STM32-F103ZET6/User/e-Paper/EPD_7in3f.c
 * since I could not find it in datasheet
 */
static EPD_Status EPD_Init_internal(void) {
	uint8_t data[6];
	EPD_Reset();
	HAL_Delay(20);
	EPD_Wait_While_Busy();
	HAL_Delay(30);

	// CMDH
	data[0] = 0x49;
	data[1] = 0x55;
	data[2] = 0x20;
	data[3] = 0x08;
	data[4] = 0x09;
	data[5] = 0x18;
	if (EPD_Send_Command_And_Data(0xAA, data, 6) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x3F;
	data[1] = 0x00;
	data[2] = 0x32;
	data[3] = 0x2A;
	data[4] = 0x0E;
	data[5] = 0x2A;
	if (EPD_Send_Command_And_Data(0x01, data, 6) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x5F;
	data[1] = 0x69;
	if (EPD_Send_Command_And_Data(0x00, data, 2) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x00;
	data[1] = 0x54;
	data[2] = 0x00;
	data[3] = 0x44;
	if (EPD_Send_Command_And_Data(0x03, data, 4) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x40;
	data[1] = 0x1F;
	data[2] = 0x1F;
	data[3] = 0x2C;
	if (EPD_Send_Command_And_Data(0x05, data, 4) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x6F;
	data[1] = 0x1F;
	data[2] = 0x16;
	data[3] = 0x25;
	if (EPD_Send_Command_And_Data(0x06, data, 4) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x6F;
	data[1] = 0x1F;
	data[2] = 0x1F;
	data[3] = 0x22;
	if (EPD_Send_Command_And_Data(0x08, data, 4) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x00;
	data[1] = 0x04;
	if (EPD_Send_Command_And_Data(0x13, data, 2) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x02;
	if (EPD_Send_Command_And_Data(0x30, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x00;
	if (EPD_Send_Command_And_Data(0x41, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x3F;
	if (EPD_Send_Command_And_Data(0x50, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x02;
	data[1] = 0x00;
	if (EPD_Send_Command_And_Data(0x60, data, 2) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x03;
	data[1] = 0x20;
	data[2] = 0x01;
	data[3] = 0xE0;
	if (EPD_Send_Command_And_Data(0x61, data, 4) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x1E;
	if (EPD_Send_Command_And_Data(0x82, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x00;
	if (EPD_Send_Command_And_Data(0x84, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x00;
	if (EPD_Send_Command_And_Data(0x86, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x2F;
	if (EPD_Send_Command_And_Data(0xE3, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x00;
	if (EPD_Send_Command_And_Data(0xE0, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	data[0] = 0x00;
	if (EPD_Send_Command_And_Data(0xE6, data, 1) != EPD_OK) {
		return EPD_INIT_INTERNAL_INIT_ERR;
	}

	return EPD_OK;
}

/**
 * Toggle the RESET input to EPD
 */
static void EPD_Reset(void) {
	EPD_RESET_HIGH();
	HAL_Delay(20);
	EPD_RESET_LOW();
	HAL_Delay(2);
	EPD_RESET_HIGH();
	HAL_Delay(20);
}

/**
 * Wait until the BUSY line becomes high
 */
static void EPD_Wait_While_Busy(void) {
	while(EPD_BUSY_READ() == GPIO_PIN_RESET) {
		HAL_Delay(1);
	}
}

/**
 * Send a command byte to E-paper display
 *
 * @param cmd	(IN)	Command byte to send
 *
 * @return	Status of sending a byte over IO channels to E-paper display
 */
static HAL_StatusTypeDef EPD_Send_Command(uint8_t cmd) {
	HAL_StatusTypeDef ret;
	EPD_CS_SELECT();
	EPD_DC_COMMAND();
	if ((ret = HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY)) != HAL_OK) {
		EPD_CS_DESELECT();
		return ret;
	}
	EPD_CS_DESELECT();
	return HAL_OK;
}

/**
 * Send a data byte to E-paper display
 *
 * @param data	(IN)	Data byte to send
 *
 * @return	Status of sending a byte over IO channels to E-paper display
 */
static HAL_StatusTypeDef EPD_Send_Data(uint8_t data) {
	HAL_StatusTypeDef ret;
	EPD_CS_SELECT();
	EPD_DC_DATA();
	if ((ret = HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY)) != HAL_OK) {
		EPD_CS_DESELECT();
		return ret;
	}
	EPD_CS_DESELECT();
	return HAL_OK;
}

/**
 * Power on EPD, refresh the image on display, and then power off
 *
 * @return	Status of E-paper display refresh operation
 */
static EPD_Status EPD_Refresh_Display_Image(void) {
	uint8_t data;

	if (EPD_Send_Command(EPD_COMMAND_POWER_ON) != HAL_OK) {
		return EPD_REFRESH_DISPLAY_ERR;
	}
	EPD_Wait_While_Busy();

	data = EPD_DATA_DATA_REFRESH;
	if (EPD_Send_Command_And_Data(EPD_COMMAND_DATA_REFRESH, &data, 1) != EPD_OK) {
		return EPD_REFRESH_DISPLAY_ERR;
	}
	EPD_Wait_While_Busy();

	data = EPD_DATA_POWER_OFF;
	if (EPD_Send_Command_And_Data(EPD_COMMAND_POWER_OFF, &data, 1) != EPD_OK) {
		return EPD_REFRESH_DISPLAY_ERR;
	}
	EPD_Wait_While_Busy();

	return EPD_OK;
}

/**
 * Helper function to send a command and one or more data bytes
 *
 * @param cmd		(IN)	command byte to send to E-paper display
 * @param data		(IN)	data bytes to send to E-paper display
 * @param data_size	(IN)	Number of data bytes to send
 *
 * @return Status of sending bytes over IO channels to E-paper display
 */
static EPD_Status EPD_Send_Command_And_Data(const uint8_t cmd,
											const uint8_t *restrict const data,
											const uint8_t data_size) {
	if (EPD_Send_Command(cmd) != HAL_OK) {
			return EPD_SEND_CMD_DATA_ERR;
	}

	for (uint8_t i = 0; i < data_size; i++) {
		if (EPD_Send_Data(data[i]) != HAL_OK) {
			return EPD_SEND_CMD_DATA_ERR;
		}
	}
	return EPD_OK;
}
