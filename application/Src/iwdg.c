#include <iwdg.h>
#include <rcc.h>

void Iwdg_Init(void)
{
    RCC->RCC_CSR.REG |= (1UL << 0);
    while ((RCC->RCC_CSR.REG & (1UL << 1)) == 0U) {
    }

    IWDG->IWDG_KR.REG = IWDG_KEY_UNLOCK;
    IWDG->IWDG_PR.REG = 4U;
    IWDG->IWDG_KR.REG = IWDG_KEY_ENABLE;
    while (IWDG->IWDG_SR.REG & 0x1U) {
    }
    IWDG->IWDG_RLR.REG = 625U;
    while (IWDG->IWDG_SR.REG & 0x2U) {
    }
    //timout = ((RLR + 1) * prescaler ) / FLSI <=> 626*64/40000 = 1s
}

void Iwdg_Feed(){
    IWDG->IWDG_KR.REG = IWDG_KEY_RELOAD;
}

