/*
 * tick.h — SysTick time base
 */

#ifndef TICK_H_
#define TICK_H_

#include <stdint.h>
#include <types.h>

status_t tick_init(uint8_t priority);
uint32_t tick_get(void);
void     delay_ms(uint32_t ms);

#endif /* TICK_H_ */
