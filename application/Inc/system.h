/*
 * system.h — system clock and reset-time setup
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <stdint.h>
#include <types.h>

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);
status_t SystemClock_Config(void);
status_t SystemClock_DeInit(void);

#endif /* SYSTEM_H_ */
