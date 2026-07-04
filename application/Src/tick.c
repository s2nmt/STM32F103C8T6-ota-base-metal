/*
 * tick.c
 */

#include <tick.h>
#include <conf.h>
#include <nvic.h>
#include <system.h>

static volatile uint32_t s_tick;

status_t tick_init(uint8_t priority)
{
    uint32_t reload = SystemCoreClock / TICK_FREQ_HZ;

    if (reload == 0U) {
        return STATUS_ERROR;
    }

    SysTick->STK_CTRL.BITS.ENABLE = 0;
    SysTick->STK_LOAD.BITS.LOAD = reload - 1U;
    SysTick->STK_VAL.REG = 0U;
    SysTick->STK_CTRL.BITS.CLKSOURCE = 1;
    SysTick->STK_CTRL.BITS.TICKINT = 1;
    nvic_systick_priority_set(priority);
    SysTick->STK_CTRL.BITS.ENABLE = 1;

    return STATUS_OK;
}

uint32_t tick_get(void)
{
    return s_tick;
}

void delay_ms(uint32_t ms)
{
    const uint32_t start = tick_get();

    while ((tick_get() - start) < ms) {
    }
}

void SysTick_Handler(void)
{
    s_tick++;
}
