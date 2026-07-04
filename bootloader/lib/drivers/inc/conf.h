/*
 * conf.h — project configuration
 */

#ifndef CONF_H_
#define CONF_H_

#define TICK_INT_PRIORITY         15U
#define TICK_FREQ_HZ              1000U

#define HSI_VALUE                 8000000UL
#define HSE_VALUE                 8000000UL

#define SYSCLK_FREQ_HZ            72000000UL
#define UART1_PCLK_HZ             SYSCLK_FREQ_HZ
#define HSE_STARTUP_TIMEOUT       0x5000U

#endif /* CONF_H_ */
