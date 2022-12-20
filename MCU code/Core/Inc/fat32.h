#ifndef INC_FAT32_H_
#define INC_FAT32_H_

#include <stdint.h>
#include "data_processing.h"

/*
 * The current implementation only supports 1024 sized clusters. Since
 * SECTOR_SIZE is 512, this has the value 2
 */
#define SECTORS_PER_CLUSTER	(2)

/**
 * Return codes to expect when using FAT32 APIs
 */
typedef enum {
	FAT32_OK,                                   /**< FAT32_OK */
	FAT32_INIT_PARTITION_DISCOVERY_ERR,         /**< FAT32_INIT_PARTITION_DISCOVERY_ERR */
	FAT32_INIT_PARTITION_READ_ERR,              /**< FAT32_INIT_PARTITION_READ_ERR */
	FAT32_INIT_UNSUPPORTED_SECTOR_SIZE,         /**< FAT32_INIT_UNSUPPORTED_SECTOR_SIZE */
	FAT32_INIT_UNSUPPORTED_FAT_COUNT,           /**< FAT32_INIT_UNSUPPORTED_FAT_COUNT */
	FAT32_INIT_INVALID_EBPB_SIGNATURE,          /**< FAT32_INIT_INVALID_EBPB_SIGNATURE */
	FAT32_INIT_INVALID_BOOT_PARTITION_SIGNATURE,/**< FAT32_INIT_INVALID_BOOT_PARTITION_SIGNATURE */
	FAT32_INIT_UNSUPPORTED_SECTORS_PER_CLUSTER, /**< FAT32_INIT_UNSUPPORTED_SECTORS_PER_CLUSTER */
	FAT32_READ_FILE_NOT_FOUND,                  /**< FAT32_READ_FILE_NOT_FOUND */
	FAT32_READ_FILE_ERR,                        /**< FAT32_READ_FILE_ERR */
	FAT32_READ_FAT_READ_ERR,                    /**< FAT32_READ_FAT_READ_ERR */
	FAT32_READ_FILE_DATA_PROCESS_ERR,           /**< FAT32_READ_FILE_DATA_PROCESS_ERR */
} FAT32_Status;

/**
 * Initialize internal data structures for using FAT32 filesystem
 *
 * @return Status for FAT32 initialization operation
 */
FAT32_Status FAT32_Init(void);

/**
 * Read data from a file from root directory and call the data processing
 * callback function with each block of partial data read from the file into
 * user provided buffer
 *
 * @param filename	(IN)	Name of the file to read the data from inside the
 * 							root directory
 * @param buffer	(OUT)	Buffer to read the partial data into. Should be large
 * 							enough to support 1 cluster worth of data
 * @param cb		(IN)	Function to call to process each block of partial
 * 							data read from the file
 *
 * @return	Status for finding, reading and processing each block of data from
 * 			the file corresponding to the provided filename
 */
FAT32_Status FAT32_Read_File_From_Root_Dir_And_Process_Data(
	const char *restrict const filename,
	uint8_t *restrict const buffer,
	DataBufferProcessingCallback cb);

#endif /* INC_FAT32_H_ */
