#ifndef INC_EPD_H_
#define INC_EPD_H_

#include "data_processing.h"

/*
 * EPD display used is 800x480 pixels
 */
#define EPD_WIDTH_PIXELS	(800)
#define EPD_HEIGHT_PIXELS	(480)

/*
 * The framebuffer to use for EPD display is 400x480 bytes wide due to 2 pixels per byte.
 * https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT_(F)_Manual#Programming_Principles
 */
#define EPD_WIDTH			(EPD_WIDTH_PIXELS/2)
#define EPD_HEIGHT			(EPD_HEIGHT_PIXELS)


/**
 * Colors supported by E-paper display
 * https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT_(F)_Manual#Programming_Principles
 */
typedef enum {
	BLACK = 0,/**< BLACK */
	WHITE,    /**< WHITE */
	GREEN,    /**< GREEN */
	BLUE,     /**< BLUE */
	RED,      /**< RED */
	YELLOW,   /**< YELLOW */
	ORANGE,   /**< ORANGE */
} EPD_Color_t;

/**
 * Return codes to expect when using EPD APIs
 */
typedef enum {
	EPD_OK,/**< EPD_OK */      /**< EPD_OK */
	EPD_INIT_IO_INIT_ERR,     /**< EPD_INIT_IO_INIT_FAIL */
	EPD_INIT_INTERNAL_INIT_ERR,/**< EPD_INIT_INTERNAL_INIT_ERR */
	EPD_SEND_CMD_DATA_ERR,     /**< EPD_SEND_CMD_DATA_ERR */
	EPD_SEND_DATA_ERR,         /**< EPD_SEND_DATA_ERR */
	EPD_SEND_CMD_ERR,          /**< EPD_SEND_CMD_ERR */
	EPD_REFRESH_DISPLAY_ERR,   /**< EPD_REFRESH_DISPLAY_ERR */
	EPD_SLEEP_DATA_SEND_ERR,   /**< EPD_SLEEP_DATA_SEND_ERR */
	EPD_DEINIT_IO_DEINIT_ERR,  /**<EPD_DEINIT_IO_DEINIT_ERR */
} EPD_Status;


/**
 * Initialize the E-paper display
 *
 * @return Status for the E-paper initialization display process
 */
EPD_Status EPD_Init(void);

/**
 * Fill the complete display with given color
 *
 * @param color	(IN)	Color to fill the display screen with
 *
 * @return	Status of E-paper display clear operation
 */
EPD_Status EPD_Display_Clear(const EPD_Color_t color);

/**
 * Display an image on the full E-paper display screen
 *
 * @param img	(IN)	Bytes representing image data to show on the full display.
 * 						The size of this array should contain exactly the number of
 * 						pixels to display on the screen
 *
 * @return	Status of operation to display the provided image
 */
EPD_Status EPD_Display_Full_Image(const uint8_t *restrict const img);

/**
 * Callback function that can be used to display a full image with data provided
 * few bytes at a time. The callback function should still be provided all the
 * bytes equal to the number of pixels on the display (by maybe calling multiple
 * times)
 *
 * @param data_offset	(IN)	Offset of the first data byte in `image_buffer`
 * 								in the full image array
 * @param image_buffer	(IN)	Buffer containing partial image data
 * @param buffer_size	(IN)	Size of the buffer for partial image data
 *
 * @return	Status of operation to send partial image data to E-paper display, or
 * 			Status of operation to display the provided image when all the image
 * 			data is sent. If an error is returned, start the sequence of calls
 * 			from offset 0 again.
 */
DataProcessingStatus EPD_Display_Image_Callback(
	const uint32_t data_offset,
	const uint8_t *restrict const image_buffer,
	const uint32_t buffer_size);

/**
 * Put the E-paper display to deep sleep to conserve power
 */
EPD_Status EPD_Put_To_Sleep(void);

/**
 * De-initialize E-paper display
 *
 * @return	Status of de-initialization operation
 */
EPD_Status EPD_De_Init(void);

#endif /* INC_EPD_H_ */
