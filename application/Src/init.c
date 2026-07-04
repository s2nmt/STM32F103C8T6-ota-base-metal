/*
 * init.c
 *
 * Khoi tao nen MCU — goi dau tien trong main(), truoc SystemClock_Config().
 * Sau reset chip chay HSI 8 MHz (startup da goi SystemInit).
 *
 * init() thuc hien:
 *   - nvic_priority_group_config : chia uu tien ngat truoc khi bat IRQ
 *   - tick_init                  : SysTick 1 ms cho delay_ms / timeout
 *   - msp_init                   : hook phan cung board (weak, mac dinh rong)
 *
 * Khong doi PLL/HSE, khong init GPIO/UART — lam o buoc sau.
 */

#include <init.h>
#include <conf.h>
#include <msp.h>
#include <nvic.h>
#include <tick.h>

status_t init(void)
{
    nvic_priority_group_config(NVIC_PRIORITYGROUP_4);

    if (tick_init(TICK_INT_PRIORITY) != STATUS_OK) {
        return STATUS_ERROR;
    }

    msp_init();

    return STATUS_OK;
}
