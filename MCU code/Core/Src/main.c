#include <stdio.h>
#include <string.h>
#include "stm32l4xx_hal.h"
#include "main.h"
#include "epd.h"
#include "sdcard.h"
#include "fat32.h"
#include "led.h"
#include "rtc_and_pwr.h"
#include "logging.h"

static Boolean SystemClockConfig(void);
static void Early_Stage_Error_Handler(void);
static void Configure_For_Low_Power(void);

// Data buffer to store 1 cluster worth of data when reading file from SD card
// and processing it
static uint8_t data_buffer[SECTOR_SIZE * SECTORS_PER_CLUSTER];

static char filename_buffer[FILENAME_MAX_LENGTH];

int main(void) {
	Boolean is_bootup_from_lpm;
	uint32_t filename_counter;
	FAT32_Status fat32_ret;

	if (HAL_Init() != HAL_OK) {
		Early_Stage_Error_Handler();
	}

	Error_LED_Init();

	if (SystemClockConfig() != TRUE) {
		Error_Handler();
	}

	Logger_Init();

	if (RTC_Init() != RTC_OK) {
		Log_Msg("Error initializing RTC peripheral");
		Error_Handler();
	}

	Busy_LED_Init();

	Log_Msg("Starting application!!");

	PWR_Handle_Boot_From_Low_Power_Mode(&is_bootup_from_lpm);

	if (is_bootup_from_lpm == TRUE) {
		Log_Msg("Booting up from low power mode");
		filename_counter = RTC_Read_Backup_Register(FILENAME_COUNTER_BKUP_REG);
	}
	else {
		filename_counter = 0;	// Start with filenames from 0.bin
	}

	snprintf(filename_buffer, FILENAME_MAX_LENGTH, "%lu.bin", filename_counter);

	// Light up the LED to indicate that the chip is starting main work
	Busy_LED_Indicate_Work_Start();

	if (EPD_Init() != EPD_OK) {
		Log_Msg("Error initializing E-paper display");
		Error_Handler();
	}
	Log_Msg("E-paper display initialized");


	if (EPD_Display_Clear(WHITE) != EPD_OK) {
		Log_Msg("Error clearing E-paper display screen");
		Error_Handler();
	}

	if (SDC_Init() != SDC_OK) {
		Log_Msg("Error initializing SD card");
		Error_Handler();
	}
	Log_Msg("SD card initialized!!");

	if (FAT32_Init() != FAT32_OK) {
		Log_Msg("Error initializing FAT32 module");
		Error_Handler();
	}
	Log_Msg("FAT32 initialized!!");

	fat32_ret = FAT32_Read_File_From_Root_Dir_And_Process_Data(filename_buffer,
			data_buffer, &EPD_Display_Image_Callback);
	if ((filename_counter > 0) && (fat32_ret == FAT32_READ_FILE_NOT_FOUND)) {
		// We ran out of all the files to display. Restart from 0.bin
		filename_counter = 0;
		snprintf(filename_buffer, FILENAME_MAX_LENGTH, "%lu.bin", filename_counter);
		fat32_ret = FAT32_Read_File_From_Root_Dir_And_Process_Data(filename_buffer,
					data_buffer, &EPD_Display_Image_Callback);
	}
	if (fat32_ret != FAT32_OK) {
		Log_Msg("Error reading file %s and displaying image!", filename_buffer);
		Error_Handler();
	}

	if (SDC_Power_Off() != SDC_OK) {
		Log_Msg("Error powering off SD card");
		Error_Handler();
	}

	HAL_Delay(2000);

	if (EPD_Put_To_Sleep() != EPD_OK) {
		Log_Msg("Error putting E-paper display to sleep");
		Error_Handler();
	}

	// Dim the LED to indicate that the chip has finished main work
	Busy_LED_Indicate_Work_End();

	if (EPD_De_Init() != EPD_OK) {
		Log_Msg("Error de-initializing E-paper display");
		Error_Handler();
	}

	Busy_LED_De_Init();

	// Increment the filename counter for reading the next file after exiting
	// sleep mode and store it in corresponding backup register
	RTC_Write_Backup_Register(FILENAME_COUNTER_BKUP_REG, filename_counter + 1);

	if (RTC_Set_WakeUp_Timer(SECONDS_TO_SPEND_IN_LOW_POWER_MODE) != RTC_OK) {
		Log_Msg("Could not set up RTC Wakeup timer for %d seconds",
				SECONDS_TO_SPEND_IN_LOW_POWER_MODE);
		Error_Handler();
	}

	Error_LED_De_Init();

	Configure_For_Low_Power();

	while(1) {
		PWR_Enter_Low_Power_Mode();
	}
}

/**
 * Configure system clock and oscillators.
 * Values used here were derived from CubeMX clock config to achieve 80MHz for
 * system clock
 *
 * @return	True if the initialization is successful. False otherwise.
 */
static Boolean SystemClockConfig(void) {
	RCC_OscInitTypeDef osc_init = {0};
	RCC_ClkInitTypeDef clk_init = {0};
	const uint32_t flash_latency = FLASH_LATENCY_4;	// Table 11 in reference manual

	// Use LSE for standby clock and MSI for system + peripheral clock
	osc_init.OscillatorType = (RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI);

	// Configure MSI for 4MHz
	osc_init.MSIState = RCC_MSI_ON;
	osc_init.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	osc_init.MSIClockRange = RCC_MSIRANGE_6;

	// Configure LSE for clock in standby mode
	osc_init.LSEState = RCC_LSE_ON;

	// Configure PLL to generate 80MHz clock
	osc_init.PLL.PLLState = RCC_PLL_ON;
	osc_init.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	osc_init.PLL.PLLM = 1;
	osc_init.PLL.PLLN = 40;
	osc_init.PLL.PLLR = RCC_PLLR_DIV2;

	if (HAL_RCC_OscConfig(&osc_init) != HAL_OK) {
		return FALSE;
	}

	// Configure PLL as the clock for system + peripherals
	clk_init.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
						  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
	clk_init.APB1CLKDivider = RCC_HCLK_DIV1;
	clk_init.APB2CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&clk_init, flash_latency) != HAL_OK) {
		return FALSE;
	}

	// Update systick config according to the new clock source
	if (HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000) != HAL_OK) {
		return FALSE;
	}
	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

	return TRUE;
}

/**
 * Error handler to use until system clock and error LED are not initialized
 */
static void Early_Stage_Error_Handler(void) {
	while(1);
}

/**
 * Error handler to use after system clock and error LED are initialized
 */
void Error_Handler(void) {
	Error_LED_Indicate_Error();
	while(1);
}

/**
 * Configure the MCU to consume the least amount of current when sleeping, in
 * order to extend battery life
 */
static void Configure_For_Low_Power(void) {
	// Put *UNUSED* GPIO Pins in Analog mode and disable clock to
	// them to save power: Application note 4899, Section 7
	// Section 8.5.1 mentions that after reset the GPIO pins are put in Analog
	// mode. So we don't need to change that

	// Disable clock for GPIO peripherals
	__HAL_RCC_GPIOA_CLK_DISABLE();
	__HAL_RCC_GPIOD_CLK_DISABLE();
}
