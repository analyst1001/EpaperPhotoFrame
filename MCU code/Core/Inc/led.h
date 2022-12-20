#ifndef INC_LED_H_
#define INC_LED_H_

/**
 * Initialize peripherals for LED to indicate if the system is busy or not
 */
void Busy_LED_Init(void);

/**
 * Change the LED state to indicate that the system has started doing work
 */
void Busy_LED_Indicate_Work_Start(void);

/**
 * Change the LED state to indicate that the system has stopped doing work
 */
void Busy_LED_Indicate_Work_End(void);

/**
 * Initialize peripherals for LED to indicate if a system error occurred
 */
void Error_LED_Init(void);

/**
 * Change the LED state to indicate that a system error has occurred
 */
void Error_LED_Indicate_Error(void);

/**
 * De-initialize peripherals for LED which indicates that the system is busy
 */
void Busy_LED_De_Init(void);

/**
 * De-initialize peripherals for LED which indicates that system error occurred
 */
void Error_LED_De_Init(void);

#endif /* INC_LED_H_ */
