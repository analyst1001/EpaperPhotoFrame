#ifndef INC_MAIN_H_
#define INC_MAIN_H_

// As per E-paper display documentation, the screen should be refreshed after
// at least 180s and at least once per 24h:
// https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT_(F)_Manual#Precautions
#define SECONDS_TO_SPEND_IN_LOW_POWER_MODE	(200-1)

// Files are expected to be stores as 1.bin, 2.bin, etc. This backup register
// keeps track of the filenames when the MCU goes to sleep mode
#define FILENAME_COUNTER_BKUP_REG	(0)

// uint32_t can store 2**32-1 = 4294967295
// So the largest filename can be 4294967295.bin, which leads to 15 bytes
// including the NULL byte
#define FILENAME_MAX_LENGTH		(15)

typedef enum {
	TRUE,
	FALSE,
} Boolean;

/**
 * Subroutine to use when a system level error occurs
 */
void Error_Handler(void);

#endif /* INC_MAIN_H_ */
