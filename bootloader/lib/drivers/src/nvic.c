/*
 * nvic.c
 */

#include <nvic.h>

#define SCB_AIRCR           (*(volatile uint32_t *)0xE000ED0CUL)
#define SCB_SHPR3           (*(volatile uint32_t *)0xE000ED20UL)
#define SCB_AIRCR_VECTKEY   (0x05FAUL << 16U)
#define SCB_AIRCR_PRIGROUP_MSK   (0x7UL << 8)

void nvic_priority_group_config(nvic_priority_group_t group)
{
    SCB_AIRCR = (SCB_AIRCR & ~(SCB_AIRCR_VECTKEY | SCB_AIRCR_PRIGROUP_MSK))
              | SCB_AIRCR_VECTKEY
              | ((uint32_t)group << 8);
}

void nvic_irq_priority_set(IRQn_Type irqn, uint8_t preempt, uint8_t sub)
{
    uint32_t priority = ((uint32_t)preempt << 4) | ((uint32_t)sub & 0x0FU);
    volatile uint8_t *ipr = (volatile uint8_t *)&NVIC->NVIC_IPR[0];

    ipr[(uint32_t)irqn] = (uint8_t)priority;
}

void nvic_systick_priority_set(uint8_t priority)
{
    SCB_SHPR3 = (SCB_SHPR3 & 0x00FFFFFFUL) | ((uint32_t)priority << 24);
}

void nvic_irq_enable(IRQn_Type irqn)
{
    NVIC->NVIC_ISER[(uint32_t)irqn / 32U].REG = (1UL << ((uint32_t)irqn % 32U));
}

void nvic_irq_disable(IRQn_Type irqn)
{
    NVIC->NVIC_ICER[(uint32_t)irqn / 32U].REG = (1UL << ((uint32_t)irqn % 32U));
}
