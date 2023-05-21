#include "stm32l4xx_hal.h"
#include "rtc_and_pwr.h"

// Use RTC peripheral for wakeup timer
static RTC_HandleTypeDef  hrtc;

RTC_Status RTC_Init(void) {
    RCC_PeriphCLKInitTypeDef rtc_clk_init = {0};

    // Initialize clock source for RTC
    rtc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    rtc_clk_init.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&rtc_clk_init) != HAL_OK) {
        return RTC_CLK_CFG_ERR;
    }

    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;    // Use the default value of 128
    hrtc.Init.SynchPrediv = 255;    // Use the default value of 256
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity =  RTC_OUTPUT_POLARITY_LOW;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    if (HAL_RTC_Init(&hrtc) != HAL_OK) {
        return RTC_INIT_ERR;
    }
    return RTC_OK;
}


void PWR_Enter_Low_Power_Mode(void) {
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    HAL_PWR_EnterSTANDBYMode();
}


void PWR_Handle_Boot_From_Low_Power_Mode(
        Boolean *restrict const is_boot_from_lpm) {
    // Assume that MCU is not booting up from Low-power mode
    *is_boot_from_lpm = FALSE;

    __HAL_RCC_PWR_CLK_ENABLE();

    if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET) {
        *is_boot_from_lpm = TRUE;
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    }
}

RTC_Status RTC_Set_WakeUp_Timer(const uint32_t seconds_to_sleep) {
    // 1s to 18 hrs with 16 bits counter
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds_to_sleep,
            RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
        return RTC_WAKEUP_TIMER_SETUP_ERR;
    }
    return RTC_OK;
}

uint32_t RTC_Read_Backup_Register(const uint32_t backup_register) {
    return HAL_RTCEx_BKUPRead(&hrtc, backup_register);
}

void RTC_Write_Backup_Register(const uint32_t backup_register, const uint32_t value) {
    HAL_RTCEx_BKUPWrite(&hrtc, backup_register, value);
}

/**
 * Low-level RTC peripheral initialization
 */
void RTC_Msp_Init(void) {
    __HAL_RCC_RTC_ENABLE();
}
