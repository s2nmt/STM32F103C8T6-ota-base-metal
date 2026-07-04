/*
 * nvic.h
 */

#ifndef NVIC_H_
#define NVIC_H_

#include <stdint.h>

typedef struct {
    union {
        unsigned long REG;
        struct {
            unsigned long ENABLE : 1;
            unsigned long TICKINT : 1;
            unsigned long CLKSOURCE : 1;
            unsigned long reserved_1 :13;
            unsigned long COUNTFLAG : 1;
            unsigned long reserved_2 : 15;
        }BITS;
    } STK_CTRL;

    union {
        unsigned long REG;
        struct {
            unsigned long LOAD : 24;
            unsigned long reserved : 8;
        }BITS;
    } STK_LOAD;

    union  {
        unsigned long REG;
        struct {
            unsigned long CURRENT : 24;
            unsigned long reserved : 8;
        }BITS;
    } STK_VAL;

    union  {
        unsigned long REG;
        struct {
            unsigned long CALIB : 24;
            unsigned long reserved : 8;
        }BITS;
    } STK_CALIB;
} SysTick_TypeDef;

typedef struct {
    union  {
        unsigned long REG;
    } NVIC_ISER[3];

    unsigned long RESERVED0[30];

    union {
        unsigned long REG;
    } NVIC_ICER[3];

    unsigned long RESERVED1[30];

    union {
        unsigned long REG;
    } NVIC_ISPR[3];

    unsigned long RESERVED2[30];

    union {
        unsigned long REG;
    } NVIC_ICPR[3];

    unsigned long RESERVED3[30];

    union {
        unsigned long REG;
    } NVIC_IABR[3];

    unsigned long RESERVED4[30];

    union {
        unsigned long REG;
    } NVIC_IPR[21];

} NVIC_TypeDef;

#define SysTick ((SysTick_TypeDef *)0xE000E010UL)
#define NVIC    ((NVIC_TypeDef *)0xE000E100UL)

typedef enum
{
    WWDG_IRQn               = 0,
    PVD_IRQn                = 1,
    TAMPER_IRQn             = 2,
    RTC_IRQn                = 3,
    FLASH_IRQn              = 4,
    RCC_IRQn                = 5,
    EXTI0_IRQn              = 6,
    EXTI1_IRQn              = 7,
    EXTI2_IRQn              = 8,
    EXTI3_IRQn              = 9,
    EXTI4_IRQn              = 10,
    DMA1_Channel1_IRQn      = 11,
    DMA1_Channel2_IRQn      = 12,
    DMA1_Channel3_IRQn      = 13,
    DMA1_Channel4_IRQn      = 14,
    DMA1_Channel5_IRQn      = 15,
    DMA1_Channel6_IRQn      = 16,
    DMA1_Channel7_IRQn      = 17,
    ADC1_2_IRQn             = 18,
    USB_HP_CAN1_TX_IRQn     = 19,
    USB_LP_CAN1_RX0_IRQn    = 20,
    CAN1_RX1_IRQn           = 21,
    CAN1_SCE_IRQn           = 22,
    EXTI9_5_IRQn            = 23,
    TIM1_BRK_IRQn           = 24,
    TIM1_UP_IRQn            = 25,
    TIM1_TRG_COM_IRQn       = 26,
    TIM1_CC_IRQn            = 27,
    TIM2_IRQn               = 28,
    TIM3_IRQn               = 29,
    TIM4_IRQn               = 30,
    I2C1_EV_IRQn            = 31,
    I2C1_ER_IRQn            = 32,
    I2C2_EV_IRQn            = 33,
    I2C2_ER_IRQn            = 34,
    SPI1_IRQn               = 35,
    SPI2_IRQn               = 36,
    USART1_IRQn             = 37,
    USART2_IRQn             = 38,
    USART3_IRQn             = 39,
    EXTI15_10_IRQn          = 40,
    RTCAlarm_IRQn           = 41,
    USBWakeUp_IRQn          = 42,
} IRQn_Type;

typedef enum {
    NVIC_PRIORITYGROUP_0 = 0x00000007U,
    NVIC_PRIORITYGROUP_1 = 0x00000006U,
    NVIC_PRIORITYGROUP_2 = 0x00000005U,
    NVIC_PRIORITYGROUP_3 = 0x00000004U,
    NVIC_PRIORITYGROUP_4 = 0x00000003U,
} nvic_priority_group_t;

void nvic_irq_enable(IRQn_Type irqn);
void nvic_irq_disable(IRQn_Type irqn);
void nvic_irq_priority_set(IRQn_Type irqn, uint8_t preempt, uint8_t sub);
void nvic_priority_group_config(nvic_priority_group_t group);
void nvic_systick_priority_set(uint8_t priority);

#endif /* NVIC_H_ */
