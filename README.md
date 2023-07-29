# E-Paper Photo Frame

## Goals
- The main goal is to display photos on a E-paper display so that it looks similar to a photo frame. The displayed photo will change periodically.
- An additional goal is to allow the device to run on a common battery power source so that it can work without plugging into wall power.

## How to use
The script stored in `Software/transform_images.py` can be used to modify images in common format (png, jpg, etc.) to binary blobs which can be copied to a FAT32 partition (formatted with block size 512) on a SD card. These blobs are read by software starting with filename "0.bin", "1.bin", and so on and displayed on the E-paper screen. When a filename "<n>.bin" for some number `n` is not found, the software starts displaying images from "0.bin" again and keeps looping like this.

## Branches
This branch has PCB design to hold all the components for E-paper photo frame with connections to external battery, external E-paper display, external LEDs, and external SD card storage. Additionally it has code that can be flashed to STM32 MCU to use the PCB as an Epaper photo frame.
The branch `development_board` has code to run software on the development board with appropriate connections to external devices.

## MCU interfaces
- SPI interface is used to communicate with SD card and E-paper display.
- UART interface is used to log debug messages. This can be disabled by a C macro to reduce power usage in the final version.
- GPIOs to power on/off BUSY and ERROR LEDs, and SD card.

## Code implementation
1. Initialize the MCU using HAL layer. Configure system clock to use 80MHz. Using the max frequency allows SPI interface to SD card to communicate with max speed. This will allow SD card to be powered on for a shorter amount of time hence reducing the power usage (given that I plan to keep the MCU in low power mode for hours).
2. Initialize LEDs and RTC peripheral. BUSY LED indicates that the MCU is busy using the SD card or the E-paper display. ERROR LED indicates that an irrecoverable error has occurred. RTC peripheral is used to leverage the WakeUp timer in order to wake up the MCU from sleep mode periodically.
3. Initialize SD card and E-paper display.
4. Read data from the file to display and send it to E-paper display. Refresh the display and put it to sleep when the image is displayed.
5. Enable WakeUp interrupt to wake up the processor after given time interval.
6. Configure peripherals to reduce current consumption and put the MCU in Standby/Shutdown mode.

## Current consumption (TBD)

## Future Plans
- Use a data logger to measure instantaneous current consumption value with better resolution.
- Re-write the logic to use interrupt based processing. The SPI based communication can use interrupt based processing where the MCU can be put into low-power sleep mode when waiting for data transmission/reception. Same can be used when waiting for BUSY signal from E-paper display and time delays in the logic.
- Use DMA peripheral to transmit/receive large data blobs (image pixel data). STM32L4 series processor support a feature called Batch Acquisition sub-mode(BAM), as mentioned in Section 2.1.1 of [AN4621](https://www.st.com/resource/en/application_note/an4621-stm32l4-and-stm32l4-ultralowpower-features-overview-stmicroelectronics.pdf), that could allow this.