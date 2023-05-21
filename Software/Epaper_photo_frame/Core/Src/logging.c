#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "stm32l4xx_hal.h"
#include "logging.h"
#include "main.h"

#ifdef LOG_ENABLED

// Use SWO pin for logging messages

void Logger_Init(void) {
    SET_BIT(CoreDebug->DEMCR, CoreDebug_DEMCR_TRCENA_Msk);
    SET_BIT(ITM->TCR, ITM_TCR_ITMENA_Msk);
    SET_BIT(ITM->TER, 1 << 0);
}

void Log_Msg(const char *restrict const msg, ...) {
    static char formatted_msg[MAX_LOG_MSG_SIZE];

    va_list args;
    va_start(args, msg);
    vsnprintf(formatted_msg, MAX_LOG_MSG_SIZE, msg, args);
    va_end(args);

    uint32_t i = 0;
    while (formatted_msg[i] != '\0') {
        ITM_SendChar(formatted_msg[i]);
        i++;
    }
}

#else

void Logger_Init(void) {};

void Log_Msg(const char *restrict const msg, ...) {};

#endif
