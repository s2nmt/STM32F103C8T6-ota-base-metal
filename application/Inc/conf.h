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
#define HSE_STARTUP_TIMEOUT       0x5000U

/* Phai trung linker script FLASH ORIGIN (APP1=0x08005900, APP2=0x0800AB00) */
#define APP_FLASH_ORIGIN          0x0800AB00UL

#endif /* CONF_H_ */
