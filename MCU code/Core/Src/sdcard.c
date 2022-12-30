#include <assert.h>
#include "stm32l4xx_hal.h"
#include "sdcard.h"

/*
 * Use SPI2 for communication with SD Card: NSS(PD5), SCK(PD1), MISO(PD3), MOSI(PD4)
 * Use PD0 for controlling SD Card power switch
 */
static SPI_HandleTypeDef hspi2;

/**
 * Speed to use for SPI when reading data from SD card
 */
typedef enum {
	SDC_SPI_SPEED_LOW,		// Low speed SPI clk for SD card initialization
	SDC_SPI_SPEED_HIGH,		// High speed SPI clk for reading data
} SDC_SPI_Speed;

/**
 *	Type of SD card
 */
typedef enum {
	CARD_UNKNOWN,/**< CARD_UNKNOWN */
	SDSC_V1,     /**< SDSC_V1 */
	SDSC_V2,     /**< SDSC_V2 */
	SDHC_SDXC_V2,/**< SDHC_SDXC_V2 */
} SDC_Type;


// Macros to select/deselect SD card chip while communicating
#define SDC_CS_SELECT()		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_RESET)
#define SDC_CS_DESELECT()	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET)

// Few SD card commands, their arguments and CRC values
#define CMD0				(0)
#define CMD0_ARGS			(0x00000000)
#define CMD0_CRC			(0x94)

#define CMD8				(8)
#define CMD8_ARGS			(0x000001AA)	// Check voltage range 2.7-3.6V
#define CMD8_CRC			(0x86)

#define CMD55				(55)
#define CMD55_ARGS			(0x00000000)
#define CMD55_CRC			(0)

#define CMD41				(41)
#define CMD41_ARGS_SDSC_V1	(0x00000000)
#define CMD41_ARGS_OTHERS	(0x40000000)
#define CMD41_CRC			(0)

#define CMD58				(58)
#define CMD58_ARGS			(0x00000000)
#define CMD58_CRC			(0)

#define CMD17				(17)
#define CMD17_CRC			(0)


// Store the SD card type to use when reading data from it
static SDC_Type SD_card_type;


static SDC_Status SPI2_Init(const SDC_SPI_Speed speed);
void SDC_SPI_Msp_Init(void);
static SDC_Status SDC_Init_Internal(SDC_Type *restrict const card_type);
static SDC_Status Send_CMD(const uint8_t cmd, const uint32_t args, const uint8_t crc);
static SDC_Status Receive_Response1(uint8_t *restrict const response);
static SDC_Status Do_CMD0_Init(void);
static SDC_Status Receive_Response_3_Or_7(uint8_t *restrict const reponse);
static SDC_Status Do_CMD8_Init(SDC_Type *restrict const card_type);
static SDC_Status Do_ACMD41_Init(const SDC_Type card_type);
static SDC_Status Do_CMD58_Init(SDC_Type *restrict const card_type);
static HAL_StatusTypeDef SDC_SPI_Deselect(void);
static HAL_StatusTypeDef SDC_SPI_Select(void);
static void SDC_Power_Pin_Init(void);
static void SDC_Power_Enable(void);
static void SDC_Power_Disable(void);

SDC_Status SDC_Init(void) {
	// Assume the card type is unknown until it is initialized
	SD_card_type = CARD_UNKNOWN;

	// Initialize pin to power up SD card
	SDC_Power_Pin_Init();

	// Enable power to SD card
	SDC_Power_Enable();

	HAL_Delay(10);

	SDC_Status ret;
	ret = SPI2_Init(SDC_SPI_SPEED_LOW);	// Use low speed for initial setup
	if (ret != SDC_OK) {
		return ret;
	}

	ret = SDC_Init_Internal(&SD_card_type);
	return ret;
}

SDC_Status SDC_Read_Sector(uint32_t start_addr, uint8_t *restrict const buffer) {
	uint8_t ret;
	uint8_t response;
	uint32_t start_tick;
	uint8_t crc[2] = {0xFF, 0xFF};

	// SD card initialization should have set the card type if successful
	if (SD_card_type == CARD_UNKNOWN) {
		return SDC_READ_CARD_UNSUPPORTED;
	}

	// SDSC V1 takes address in terms of byte offsets
	if (SD_card_type == SDSC_V1) {
		start_addr *= SECTOR_SIZE;
	}

	if (SDC_SPI_Select() != HAL_OK) {
		return SDC_READ_START_ERR;
	}

	ret = Send_CMD(CMD17, start_addr, CMD17_CRC);
	if (ret != SDC_OK) {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_READ_END_ERR;
		}
		return SDC_READ_SEND_CMD_ERR;
	}
	ret = Receive_Response1(&response);
	if (ret != SDC_OK) {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_READ_END_ERR;
		}
		return SDC_READ_RESPONSE1_ERR;
	}

	start_tick = HAL_GetTick();
	do {	// Wait at most 200 ms for valid data/err token to appear
		response = 0xFF;
		if (HAL_SPI_Receive(&hspi2, &response, 1, HAL_MAX_DELAY) != HAL_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_READ_END_ERR;
			}
			return SDC_READ_DATA_TOKEN_RCV_ERR;
		}
	} while (((HAL_GetTick() - start_tick) < 200) && (response == 0xFF));

	if (response == 0xFE) {
		// Valid data token
		for (uint16_t i = 0; i < SECTOR_SIZE; i++) {
			buffer[i] = 0xFF;
		}
		if (HAL_SPI_Receive(&hspi2, buffer, SECTOR_SIZE, HAL_MAX_DELAY) != HAL_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_READ_END_ERR;
			}
			return SDC_READ_DATA_RCV_ERR;
		}

		// Receive CRC tokens. We do not check for CRC errors
		if (HAL_SPI_Receive(&hspi2, crc, 2, HAL_MAX_DELAY) != HAL_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_READ_END_ERR;
			}
			return SDC_READ_DATA_CRC_RCV_ERR;
		}
	}
	else if (response == 0xFF) {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_READ_END_ERR;
		}
		return SDC_READ_DATA_TOKEN_WAIT_TIMEOUT;
	}
	else {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_READ_END_ERR;
		}
		return SDC_READ_ERROR_TOKEN_RECEIVED;
	}

	if (SDC_SPI_Deselect() != HAL_OK) {
		return SDC_READ_END_ERR;
	}

	return SDC_OK;
}

SDC_Status SDC_Power_Off(void) {
	if (HAL_SPI_DeInit(&hspi2) != HAL_OK) {
		return SDC_POWEROFF_IO_DEINIT_ERR;
	}
	HAL_Delay(1000);
	HAL_Delay(6);
	SDC_Power_Disable();
	HAL_Delay(6);
	return SDC_OK;
}

/**
 * Initialize the GPIO pin that controls power to the SD card
 */
static void SDC_Power_Pin_Init(void) {
	GPIO_InitTypeDef gpio_sdpwr = {0};

	__HAL_RCC_GPIOD_CLK_ENABLE();
	gpio_sdpwr.Pin = GPIO_PIN_0;
	gpio_sdpwr.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_sdpwr.Pull = GPIO_PULLDOWN;
	gpio_sdpwr.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &gpio_sdpwr);

	// Set the pin to pulldown during MCU sleep so that SD card remains powered
	// off
	HAL_PWREx_EnablePullUpPullDownConfig();
	// Return value should be HAL_OK since we used correct GPIO bank
	assert(HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_D, PWR_GPIO_BIT_0) == HAL_OK);
}

/**
 * Enable power to SD card
 */
static void SDC_Power_Enable(void) {
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, SET);
}

/**
 * Disable power to SD card
 */
static void SDC_Power_Disable(void) {
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, RESET);
}

/**
 * Initialize the IO channels to SD card using SPI2 peripheral from MCU
 *
 * @param speed	(IN)	Speed to use while communicating with SD card
 *
 * @return	Status of SPI2 peripheral initialization operation
 */
static SDC_Status SPI2_Init(const SDC_SPI_Speed speed) {
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	if (speed == SDC_SPI_SPEED_LOW) {
		// SPI gets fPCLK/4 = 20 MHz. Use prescaler 128 to use 156.25KHz for
		// initial communication with SD card
		hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
	}
	else if (speed == SDC_SPI_SPEED_HIGH) {
		hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	}
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

	if (HAL_SPI_Init(&hspi2) != HAL_OK) {
		return SDC_INIT_IO_INIT_ERR;
	}

	// Deselect the SD card chip to avoid spurious communication
	if (SDC_SPI_Deselect() != HAL_OK) {
		return SDC_INIT_CHIP_DESELECT_ERR;
	}

	return SDC_OK;
}

/**
 *	Perform low level initialization for SPI2 peripheral
 */
void SDC_SPI_Msp_Init(void) {
	GPIO_InitTypeDef gpio_spi = {0};

	// Use NSS(PD5), SCK(PD1), MISO(PD3), MOSI(PD4) - Table 10 in User manual with appropriate bridge solder config
	__HAL_RCC_SPI2_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	// Initialize the chip select pin
	gpio_spi.Pin = GPIO_PIN_5;
	gpio_spi.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_spi.Pull = GPIO_NOPULL;
	gpio_spi.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &gpio_spi);

	// Initialize the IO channels
	gpio_spi.Pin = GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4;
	gpio_spi.Mode = GPIO_MODE_AF_PP;
	gpio_spi.Alternate = GPIO_AF5_SPI2;
	gpio_spi.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOD, &gpio_spi);
}

/**
 * Low-level de-initialization of SPI2 peripheral used for IO channels
 */
void SDC_SPI_Msp_De_Init(void) {
	__HAL_RCC_SPI2_CLK_DISABLE();
	// Configure all IO channels as pull-up and power down the SD card. Logic is
	// based on data present in https://thecavepearlproject.org/2017/05/21/switching-off-sd-cards-for-low-power-data-logging/
	HAL_PWREx_EnablePullUpPullDownConfig();
	assert(HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_D, GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5) == HAL_OK);
	SDC_Power_Disable();
	HAL_GPIO_DeInit(GPIOD, GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);
}

/**
 * Initialize the SD card by sending commands over IO channels
 *
 * @param card_type	(OUT)	Variable to store the SD card type value when it
 * 							is confirmed during initialization
 *
 * @return	Status of SD card initialization operation
 */
static SDC_Status SDC_Init_Internal(SDC_Type *restrict const card_type) {
	uint8_t dummy_data = 0xFF;
	uint8_t ret;

	HAL_Delay(2);		// Wait for at least 1 ms

	// Send at least 74 clock ticks keeping CS high - 10 bytes = 80 clock cycles
	for (uint8_t i = 0; i < 10; i++) {
		if (HAL_SPI_Transmit(&hspi2, &dummy_data, 1, HAL_MAX_DELAY) != HAL_OK) {
			return SDC_INIT_POWER_ON_WAIT_ERR;
		}
	}

	ret = Do_CMD0_Init();
	if (ret != SDC_OK) {
		return ret;
	}

	ret = Do_CMD8_Init(card_type);
	if (ret != SDC_OK) {
		return ret;
	}

	ret = Do_ACMD41_Init(*card_type);
	if (ret != SDC_OK) {
		return ret;
	}

	if (*card_type != SDSC_V1) {
		ret = Do_CMD58_Init(card_type);
		if (ret != SDC_OK) {
			return ret;
		}
	}

	return SDC_OK;
}

/**
 * Helper function to send a command, its arguments and CRC value to SD card
 *
 * @param cmd	(IN)	Command to request on SD card
 * @param args	(IN)	Arguments to use for the requested command
 * @param crc	(IN)	CRC value to use for checking the command request
 *
 * @return	Status of sending the command related data to SD card
 */
static SDC_Status Send_CMD(const uint8_t cmd, const uint32_t args,
		const uint8_t crc) {
	static uint8_t tx_data[6];

	// Use last 6 bytes for cmd and set transmission bit
	tx_data[0] = (cmd & 0x3F) | (1 << 6);

	tx_data[1] = (args >> 24) & 0xFF;
	tx_data[2] = (args >> 16) & 0xFF;
	tx_data[3] = (args >> 8) & 0xFF;
	tx_data[4] = args & 0xFF;

	tx_data[5] = crc | 1;	// Set the end bit to 1

	if (HAL_SPI_Transmit(&hspi2, tx_data, sizeof(tx_data),
			HAL_MAX_DELAY) != HAL_OK) {
		return SDC_INIT_CMD_SEND_ERR;
	}

	return SDC_OK;
}

/**
 * Receive bytes for Response type 1
 *
 * @param response	(OUT)	Byte to store the response data
 *
 * @return	Status or receiving Response1 byte
 */
static SDC_Status Receive_Response1(uint8_t *restrict const response) {
	// Number of times to check: 0 to 8 bytes for SDC, 1 to 8 bytes for MMC
	uint8_t tries = 12;
	*response = 0xFF;	// Keep MOSI high while receiving data

	do {
		if (HAL_SPI_Receive(&hspi2, response, 1, HAL_MAX_DELAY) != HAL_OK) {
			return SDC_INIT_RESPONSE1_WAIT_RCV_ERR;
		}
		if (tries-- == 0) {
			return SDC_INIT_RESPONSE1_WAIT_TIMEOUT;
		}
	} while ((*response & 0x7F) == 0X7F);	// TODO: Check why 0xFF does not
											// work here - if power is not removed
											// from SD card and init is called again,
											// the first byte after CMD0 is 0x7F
											// followed by 0x01 - Fix code to not
											//initialize SD card without removing power
	return SDC_OK;
}

/**
 * Do CMD 0 handshake with SD card over IO channels
 *
 * @return	Status of CMD0 handshake with SD card
 */
static SDC_Status Do_CMD0_Init(void) {
	uint8_t response;
	uint8_t ret;
	uint8_t tries = 10;

	if (SDC_SPI_Select() != HAL_OK) {
		return SDC_INIT_CMD0_START_ERR;
	}

	do {
		// Send CMD0 request
		ret = Send_CMD(CMD0, CMD0_ARGS, CMD0_CRC);
		if (ret != SDC_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_INIT_CMD0_END_ERR;
			}
			return ret;
		}
		// Wait for R1 response
		ret = Receive_Response1(&response);
		if (ret != SDC_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_INIT_CMD0_END_ERR;
			}
			return ret;
		}
		// If we fail a couple of times, mark the initialization as failure
		if (tries-- == 0) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_INIT_CMD0_END_ERR;
			}
			return SDC_INIT_CMD0_TIMEOUT;
		}
	} while (response != 0x01);

	if (SDC_SPI_Deselect() != HAL_OK) {
		return SDC_INIT_CMD0_END_ERR;
	}
	return SDC_OK;
}

/**
 * Receive bytes for Response type 3 or 7
 *
 * @param response	(OUT)	Buffer to store the response in. Buffer should be
 * 							large enough to hold the entire reponse
 *
 * @return	Status of receiving the response bytes
 */
static SDC_Status Receive_Response_3_Or_7(uint8_t *restrict const response) {
	uint8_t ret;

	for (uint8_t i = 0; i < 5; i++) {
		response[i] = 0xFF;		// Keep MOSI high while receiving data
	}

	ret = Receive_Response1(&response[0]);
	if (ret != SDC_OK) {
		return ret;
	}
	if (response[0] > 1) {	// Any bit except idle indicates unexpected behavior
		return SDC_INIT_RESPONSE37_RESPONSE1_ERR;
	}

	if (HAL_SPI_Receive(&hspi2, &response[1], 4, HAL_MAX_DELAY) != HAL_OK) {
		return SDC_INIT_RESPONSE37_POST_RESPONSE1_RECV_ERR;
	}

	return SDC_OK;
}

/**
 * Do CMD8 handshake with SD card over IO channels
 *
 * @param card_type	(OUT)	Variable to store the card type when it is determined
 * 							during initialization
 *
 * @return	Status of CMD8 handshake with SD card
 */
static SDC_Status Do_CMD8_Init(SDC_Type *restrict const card_type) {
	uint8_t ret;
	uint8_t response[5];

	if (SDC_SPI_Select() != HAL_OK) {
		return SDC_INIT_CMD8_START_ERR;
	}

	ret = Send_CMD(CMD8, CMD8_ARGS, CMD8_CRC);
	if (ret != SDC_OK) {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_INIT_CMD8_END_ERR;
		}
		return ret;
	}

	ret = Receive_Response_3_Or_7(response);
	if (ret != SDC_OK) {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_INIT_CMD8_END_ERR;
		}
		return ret;
	}

	if (SDC_SPI_Deselect() != HAL_OK) {
		return SDC_INIT_CMD8_END_ERR;
	}

	// Check all the response bytes
	if (response[0] == 0x1) {
		// Only idle bit set in response 1 => SDv2 or later
		if (response[3] != 0x1) {
			// Voltage range not supported
			return SDC_INIT_VOLTAGE_UNSUPPORTED;
		}
		if (response[4] != 0xAA) {
			// Pattern mismatch
			return SDC_INIT_PATTERN_MISMATCH;
		}
	}
	else if (response[0] == 0x5) {
		// Idle and illegal command bits set => SDv1 or MMC
		*card_type = SDSC_V1;
	}
	else {
		return SDC_INIT_CARD_UNSUPPORTED;
	}

	// Set SPI to use 10MHz clock for faster transfers
	hspi2.Instance->CR1 = (hspi2.Instance->CR1 & ~SPI_CR1_BR_Msk) | SPI_BAUDRATEPRESCALER_2;
	return SDC_OK;
}

/**
 * Do ACMD41 handshake with SD card over IO channels
 *
 * @param card_type	(IN)	Type of SD card as determined in the previous
 * 							initialization steps
 *
 * @return	Status of ACMD41 handshake with SD card
 */
static SDC_Status Do_ACMD41_Init(const SDC_Type card_type) {
	uint8_t ret;
	uint8_t tries = 100; // 100 tries with 10 ms wait each = 1s wait for response
	uint8_t response = 0xFF;

	if (SDC_SPI_Select() != HAL_OK) {
		return SDC_INIT_ACMD41_START_ERR;
	}

	do {
		ret = Send_CMD(CMD55, CMD55_ARGS, CMD55_CRC);
		if (ret != SDC_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_INIT_ACMD41_END_ERR;
			}
			return ret;
		}
		ret = Receive_Response1(&response);
		if (ret != SDC_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_INIT_ACMD41_END_ERR;
			}
			return ret;
		}

		if (card_type == SDSC_V1) {
			ret = Send_CMD(CMD41, CMD41_ARGS_SDSC_V1, CMD41_CRC);
		}
		else {
			ret = Send_CMD(CMD41, CMD41_ARGS_OTHERS, CMD41_CRC);
		}
		if (ret != SDC_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_INIT_ACMD41_END_ERR;
			}
			return ret;
		}
		ret = Receive_Response1(&response);
		if (ret != SDC_OK) {
			if (SDC_SPI_Deselect() != HAL_OK) {
				return SDC_INIT_ACMD41_END_ERR;
			}
			return ret;
		}

		// Wait for 10 ms to check initialization status again
		HAL_Delay(10);
	} while ((tries-- > 0) && (response != 0));	// Wait until card goes out of
												// idle state or 1s
	if (SDC_SPI_Deselect() != HAL_OK) {
		return SDC_INIT_ACMD41_END_ERR;
	}

	if (tries == 0) {
		// Either MMC or unknown card type
		return SDC_INIT_CARD_UNSUPPORTED;
	}
	return SDC_OK;
}

/**
 * Do CMD58 handshake with SD card over IO channels
 *
 * @param card_type	(OUT)	Variable to store SD card type when determined during
 * 							initialization process
 *
 * @return	Status of CMD58 handshake with SD card
 */
static SDC_Status Do_CMD58_Init(SDC_Type *restrict const card_type) {
	uint8_t ret;
	uint8_t response[5];

	if (SDC_SPI_Select() != HAL_OK) {
		return SDC_INIT_CMD58_START_ERR;
	}

	ret = Send_CMD(CMD58, CMD58_ARGS, CMD58_CRC);
	if (ret != SDC_OK) {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_INIT_CMD58_END_ERR;
		}
		return ret;
	}
	ret = Receive_Response_3_Or_7(response);
	if (ret != SDC_OK) {
		if (SDC_SPI_Deselect() != HAL_OK) {
			return SDC_INIT_CMD58_END_ERR;
		}
		return ret;
	}

	if (SDC_SPI_Deselect() != HAL_OK) {
		return SDC_INIT_CMD58_END_ERR;
	}

	if (!(response[1] & 0x80)) {
		// Power up bit is not set
		return SDC_INIT_POWER_UP_BIT_NOT_SET;
	}
	if (response[1] & 0x40) {
		// HC or XC card
		*card_type = SDHC_SDXC_V2;
	}
	else {
		*card_type = SDSC_V2;
	}

	return SDC_OK;
}

/**
 * Select the SD card chip for communication
 *
 * @return	Status of chip select operation
 */
static HAL_StatusTypeDef SDC_SPI_Select(void) {
	uint8_t temp = 0xFF;
	SDC_CS_SELECT();
	return HAL_SPI_Transmit(&hspi2, &temp, 1, HAL_MAX_DELAY);
}

/**
 * De-select the SD card chip for communication
 *
 * @return	Status of chip de-select operation
 */
static HAL_StatusTypeDef SDC_SPI_Deselect(void) {
	uint8_t temp = 0xFF;
	SDC_CS_DESELECT();
	return HAL_SPI_Transmit(&hspi2, &temp, 1, HAL_MAX_DELAY);
}
