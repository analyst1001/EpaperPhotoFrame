#ifndef INC_SDCARD_H_
#define INC_SDCARD_H_

#include <stdint.h>

/*
 * Sector size used commonly for SD card and FAT filesystem. Most new
 * cards will use 512 as sector size and FAT32 also supports it
 */
#define SECTOR_SIZE	(512)

/**
 * Return codes to expect from SD card APIs
 */
typedef enum {
	SDC_OK,                                     /**< SDC_OK */
	SDC_INIT_IO_INIT_ERR,                       /**< SDC_INIT_IO_INIT_ERR */
	SDC_INIT_POWER_ON_WAIT_ERR,                 /**< SDC_INIT_POWER_ON_WAIT_ERR */
	SDC_INIT_CMD_SEND_ERR,                      /**< SDC_INIT_CMD_SEND_ERR */
	SDC_INIT_CMD0_TIMEOUT,                      /**< SDC_INIT_CMD0_TIMEOUT */
	SDC_INIT_RESPONSE1_WAIT_RCV_ERR,            /**< SDC_INIT_RESPONSE1_WAIT_RCV_ERR */
	SDC_INIT_RESPONSE1_WAIT_TIMEOUT,            /**< SDC_INIT_RESPONSE1_WAIT_TIMEOUT */
	SDC_INIT_RESPONSE37_RESPONSE1_ERR,          /**< SDC_INIT_RESPONSE37_RESPONSE1_ERR */
	SDC_INIT_RESPONSE37_POST_RESPONSE1_RECV_ERR,/**< SDC_INIT_RESPONSE37_POST_RESPONSE1_RECV_ERR */
	SDC_INIT_CARD_UNSUPPORTED,                  /**< SDC_INIT_CARD_UNSUPPORTED */
	SDC_INIT_VOLTAGE_UNSUPPORTED,               /**< SDC_INIT_VOLTAGE_UNSUPPORTED */
	SDC_INIT_PATTERN_MISMATCH,                  /**< SDC_INIT_PATTERN_MISMATCH */
	SDC_INIT_POWER_UP_BIT_NOT_SET,              /**< SDC_INIT_POWER_UP_BIT_NOT_SET */
	SDC_READ_SEND_CMD_ERR,                      /**< SDC_READ_SEND_CMD_ERR */
	SDC_READ_RESPONSE1_ERR,                     /**< SDC_READ_RESPONSE1_ERR */
	SDC_READ_DATA_TOKEN_RCV_ERR,                /**< SDC_READ_DATA_TOKEN_RCV_ERR */
	SDC_READ_DATA_TOKEN_WAIT_TIMEOUT,           /**< SDC_READ_DATA_TOKEN_WAIT_TIMEOUT */
	SDC_READ_DATA_RCV_ERR,                      /**< SDC_READ_DATA_RCV_ERR */
	SDC_READ_DATA_CRC_RCV_ERR,                  /**< SDC_READ_DATA_CRC_RCV_ERR */
	SDC_READ_ERROR_TOKEN_RECEIVED,              /**< SDC_READ_ERROR_TOKEN_RECEIVED */
	SDC_READ_CARD_UNSUPPORTED,                  /**< SDC_READ_CARD_UNSUPPORTED */
	SDC_INIT_CMD0_START_ERR,					/**< SDC_INIT_CMD0_START_ERR */
	SDC_INIT_CMD0_END_ERR,						/**< SDC_INIT_CMD0_END_ERR */
	SDC_INIT_CMD8_START_ERR,					/**< SDC_INIT_CMD8_START_ERR */
	SDC_INIT_CMD8_END_ERR,						/**< SDC_INIT_CMD8_END_ERR */
	SDC_INIT_ACMD41_START_ERR,					/**< SDC_INIT_ACMD41_START_ERR */
	SDC_INIT_ACMD41_END_ERR,					/**< SDC_INIT_ACMD41_END_ERR */
	SDC_INIT_CMD58_START_ERR,					/**< SDC_INIT_CMD58_START_ERR */
	SDC_INIT_CMD58_END_ERR,						/**< SDC_INIT_CMD58_END_ERR */
	SDC_INIT_CHIP_DESELECT_ERR,					/**< SDC_INIT_CHIP_DESELECT_ERR */
	SDC_READ_START_ERR,							/**< SDC_READ_START_ERR */
	SDC_READ_END_ERR,							/**< SDC_READ_END_ERR */
	SDC_POWEROFF_IO_DEINIT_ERR,					/**< SDC_POWEROFF_IO_DEINIT_ERR */
} SDC_Status;


/**
 * Initialize the SD card and IO channels to communicate with it
 *
 * @return	Status of SD card and IO channels initialization operation
 */
SDC_Status SDC_Init(void);

/**
 * Read a sector from given address in SD card to the provided buffer
 *
 * @param start_addr (IN)	Address of the sector to read
 * @param buffer	 (OUT)	Buffer to read the sector into. It should be at least
 * 							SECTOR_SIZE in size
 *
 * @return	Status of sector read operation
 */
SDC_Status SDC_Read_Sector(uint32_t start_addr, uint8_t *restrict const buffer);

/**
 * De-initialize the SD card and power it off
 *
 * @return Status of de-initialization operation
 */
SDC_Status SDC_Power_Off(void);

#endif /* INC_SDCARD_H_ */
