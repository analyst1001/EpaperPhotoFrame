#ifndef INC_RTC_AND_PWR_H_
#define INC_RTC_AND_PWR_H_

#include "main.h"

/**
 * Return codes to expect from RTC APIs
 */
typedef enum {
	RTC_OK,                    /**< RTC_OK */
	RTC_INIT_ERR,              /**< RTC_INIT_ERR */
	RTC_CLK_CFG_ERR,           /**< RTC_CLK_CFG_ERR */
	RTC_WAKEUP_TIMER_SETUP_ERR,/**< RTC_WAKEUP_TIMER_SETUP_ERR */
} RTC_Status;

/**
 * Initialize RTC peripheral for using wake up timer
 *
 * @return	Status of RTC peripheral initialization procedure
 */
RTC_Status RTC_Init(void);

/**
 * Put MCU in low power mode
 */
void PWR_Enter_Low_Power_Mode(void);

/**
 * Check if MCU is booting up from low-power mode or not, and handle the
 * initialization if it is booting up from low-power mode
 *
 * @param is_boot_from_lpm	(OUT)	Variable to store a value in if the MCU is
 * 									booting up from low-power mode
 */
void PWR_Handle_Boot_From_Low_Power_Mode(
		Boolean *restrict const is_boot_from_lpm);

/**
 * Initialize RTC Wakeup timer to interrupt after certain amount of time
 *
 * @param seconds_to_sleep	(IN)	Number of seconds after which the interrupt
 * 									should be triggered
 *
 * @return	Status of RTC Wakeup timer initialization
 */
RTC_Status RTC_Set_WakeUp_Timer(const uint32_t seconds_to_sleep);

/**
 * Read a backup register from RTC peripheral
 *
 * @param backup_register	(IN)	Index of backup register to read
 *
 * @return	Value stored in the backup register
 */
uint32_t RTC_Read_Backup_Register(uint32_t backup_register);

/**
 * Write given value to backup register in RTC peripheral
 *
 * @param backup_register	(IN)	Index of backup register to write value in
 * @param value				(IN)	Value to write in the backup register
 */
void RTC_Write_Backup_Register(uint32_t backup_register, uint32_t value);

#endif /* INC_RTC_AND_PWR_H_ */
