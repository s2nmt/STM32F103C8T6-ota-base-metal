#ifndef IWDG_H_
#define IWDG_H_

#include <stdint.h>

typedef struct {
    union {
    	volatile uint32_t REG;
        struct {
    		volatile uint32_t KEY : 16;
    		volatile uint32_t reserved : 16;
        } BITS;
    } IWDG_KR;

    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t PR : 3;
            volatile uint32_t reserved : 29;
        } BITS;
    } IWDG_PR;

    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t RLR : 12;
            volatile uint32_t reserved : 20;
        } BITS;
    } IWDG_RLR;

    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t PVU :1;
            volatile uint32_t RVU :1;
            volatile uint32_t :30;
        } BITS;
    } IWDG_SR;

} IWDG_TypeDef;

#define IWDG ((IWDG_TypeDef *)0x40003000)
#define IWDG_KEY_ENABLE   0xCCCCU
#define IWDG_KEY_RELOAD   0xAAAAU
#define IWDG_KEY_UNLOCK   0x5555U

void Iwdg_Init();
void Iwdg_Feed();

#endif
