################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/epd.c \
../Core/Src/fat32.c \
../Core/Src/it.c \
../Core/Src/led.c \
../Core/Src/logging.c \
../Core/Src/main.c \
../Core/Src/msp.c \
../Core/Src/rtc_and_pwr.c \
../Core/Src/sdcard.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32l4xx.c 

OBJS += \
./Core/Src/epd.o \
./Core/Src/fat32.o \
./Core/Src/it.o \
./Core/Src/led.o \
./Core/Src/logging.o \
./Core/Src/main.o \
./Core/Src/msp.o \
./Core/Src/rtc_and_pwr.o \
./Core/Src/sdcard.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32l4xx.o 

C_DEPS += \
./Core/Src/epd.d \
./Core/Src/fat32.d \
./Core/Src/it.d \
./Core/Src/led.d \
./Core/Src/logging.d \
./Core/Src/main.d \
./Core/Src/msp.d \
./Core/Src/rtc_and_pwr.d \
./Core/Src/sdcard.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32l4xx.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L475xx -c -I../FATFS/Target -I../FATFS/App -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FatFs/src -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/epd.d ./Core/Src/epd.o ./Core/Src/epd.su ./Core/Src/fat32.d ./Core/Src/fat32.o ./Core/Src/fat32.su ./Core/Src/it.d ./Core/Src/it.o ./Core/Src/it.su ./Core/Src/led.d ./Core/Src/led.o ./Core/Src/led.su ./Core/Src/logging.d ./Core/Src/logging.o ./Core/Src/logging.su ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/msp.d ./Core/Src/msp.o ./Core/Src/msp.su ./Core/Src/rtc_and_pwr.d ./Core/Src/rtc_and_pwr.o ./Core/Src/rtc_and_pwr.su ./Core/Src/sdcard.d ./Core/Src/sdcard.o ./Core/Src/sdcard.su ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32l4xx.d ./Core/Src/system_stm32l4xx.o ./Core/Src/system_stm32l4xx.su

.PHONY: clean-Core-2f-Src

