#ifndef INC_LOGGING_H_
#define INC_LOGGING_H_

#define LOG_ENABLED	1

#define MAX_LOG_MSG_SIZE	((size_t) 256)

/**
 * Configure UART4 with 8N1 w/o parity for debugging purposes
 */
void Logger_Init(void);

/**
 * Log a message over UART4 peripheral
 *
 * @param msg	(IN)	Format string to print as message
 * @param ...	(IN)	Variadic arguments to use to format the log message
 */
void Log_Msg(const char *restrict const msg, ...);

#endif /* INC_LOGGING_H_ */
