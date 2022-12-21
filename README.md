# E-Paper Photo Frame

## Goals
- The main goal is to display photos on a E-paper display so that it looks similar to a photo frame. The displayed photo will change periodically.
- An additional goal is to allow the device to run on a common battery power source so that it can work without plugging into wall power.

## Component Selection
- MCU to run logic: STM32L475VG. STM32L4 series is one of the ultra low-power series of MCU in STMicroelectronics family of processors. I had a B-L475E-IOT01A development board with this MCU, so I chose that for this project.
- E-paper display: [Waveshare 7.3in 7-color E-paper display with driver board](https://www.waveshare.com/7.3inch-e-paper-hat-f.htm). I wanted to use a color E-paper display. There are new [E-paper display modules with 4096 colors](https://shopkits.eink.com/product/6-epaper-display-sc1452-foa%e3%80%90display-module-with-front-light-and-touch-function%e3%80%91/) and more, but I did not find good documentation for programming them. So I settled with the largest + cheap 7-color display I could find at the time.
- SD card adapter: I had few [SD card adapters](https://a.co/d/43eGHtL) with me. Using a SD card allows storing large number of images to display that can be copied from a personal computer on to the SD card. Also, the complete data for each image does not fit in the MCUs SRAM, so data can be read partially and updated with each part until the full image data is processed.

## MCU interfaces
- SPI interface is used to communicate with SD card and E-paper display.
- UART interface is used to log debug messages. This can be disabled by a C macro to reduce power usage in the final version.
- GPIOs to power on/off BUSY and ERROR LEDs.

## Code implementation
1. Initialize the MCU using HAL layer. Configure system clock to use 80MHz. Using the max frequency allows SPI interface to SD card to communicate with max speed. This will allow SD card to be powered on for a shorter amount of time hence reducing the power usage (given that I plan to keep the MCU in low power mode for hours).
2. Initialize LEDs and RTC peripheral. BUSY LED indicates that the MCU is busy using the SD card or the E-paper display. ERROR LED indicates that an irrecoverable error has occurred. RTC peripheral is used to leverage the WakeUp timer in order to wake up the MCU from sleep mode periodically.
3. Initialize SD card and E-paper display.
4. Read data from the file to display and send it to E-paper display. Refresh the display and put it to sleep when the image is displayed.
5. Enable WakeUp interrupt to wake up the processor after given time interval.
6. Configure peripherals to reduce current consumption and put the MCU in Standby/Shutdown mode.

## Current consumption
I used a multimeter to display the stable current values during various system states, so the values are not precise enough.
- SD card: Around 2-4mA when using the SD card and when the MCU is in low-power mode.
- E-paper display: Around 2-3mA when the E-paper display is in standby mode. Around 15mA when the display is refreshing. <2 nA when the display is shutdown.
- MCU: Around 4-5 mA while running the logic. Around 300-400mA when in standby mode. Sometimes the MCU take around 800nA when in standby mode (as mentioned in the datasheet), but this is inconsistent at the moment.

## Future Plans
- Look into why the current measurement for MCU is significantly more that the value mentioned in datasheet when it is in standby mode.
- Shutdown SD card before MCU enters low power mode. With my current setup, the SD card still used 2-3mA when the MCU enters low-power mode. Section 5.8.1 of [Physical Layer Simplified Specification V9.00](https://www.sdcard.org/downloads/pls/) mentions power off sequence for SD cards using CMD48 and CMD49. But these commands seem to be reserved in SPI mode (Section 7.3.1.3 of Physical Layer Simplified Specification V9.00). So powering the SD card using a high side switch seems to be the best option.
- Use a data logger to measure instantaneous current consumption value with better resolution.
- Re-write the logic to use interrupt based processing. The SPI based communication can use interrupt based processing where the MCU can be put into low-power sleep mode when waiting for data transmission/reception. Same can be used when waiting for BUSY signal from E-paper display and time delays in the logic.
- Use DMA peripheral to transmit/receive large data blobs (image pixel data). STM32L4 series processor support a feature called Batch Acquisition sub-mode(BAM), as mentioned in Section 2.1.1 of [AN4621](https://www.st.com/resource/en/application_note/an4621-stm32l4-and-stm32l4-ultralowpower-features-overview-stmicroelectronics.pdf), that could allow this.