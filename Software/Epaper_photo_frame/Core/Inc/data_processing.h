#ifndef INC_DATA_PROCESSING_H_
#define INC_DATA_PROCESSING_H_

/**
 * Return codes expected from data processing callback
 */
typedef enum {
	DATA_PROCESSING_OK,                     /**< DATA_PROCESSING_OK */
	DATA_PROCESSING_MORE_THAN_EXPECTED_DATA,/**< DATA_PROCESSING_MORE_THAN_EXPECTED_DATA */
	DATA_PROCESSING_PROCESS_ERR,            /**< DATA_PROCESSING_PROCESS_ERR */
} DataProcessingStatus;

typedef DataProcessingStatus (*DataBufferProcessingCallback)(
	const uint32_t data_offset,
	const uint8_t *restrict const data_buffer,
	const uint32_t data_size);

#endif /* INC_DATA_PROCESSING_H_ */
