/*
 * system.c
 */

#include <system.h>
#include <conf.h>
#include <flash.h>
#include <rcc.h>
#include <tick.h>

#define RCC_CR_HSEON            (1UL << 16)
#define RCC_CR_HSERDY           (1UL << 17)
#define RCC_CR_PLLON            (1UL << 24)
#define RCC_CR_PLLRDY           (1UL << 25)

#define RCC_CFGR_SW_Msk         (0x3UL << 0)
#define RCC_CFGR_SWS_Msk        (0x3UL << 2)
#define RCC_CFGR_HPRE_Msk       (0xFUL << 4)
#define RCC_CFGR_PPRE1_Msk      (0x7UL << 8)
#define RCC_CFGR_PPRE2_Msk      (0x7UL << 11)
#define RCC_CFGR_PLLSRC_Msk     (1UL << 16)
#define RCC_CFGR_PLLXTPRE_Msk   (1UL << 17)
#define RCC_CFGR_PLLMUL_Msk     (0xFUL << 18)

#define RCC_CFGR_SW_PLL         (0x2UL << 0)
#define RCC_CFGR_SWS_PLL        (0x2UL << 2)
#define RCC_CFGR_PPRE1_DIV2     (0x4UL << 8)
#define RCC_CFGR_PLLSRC_HSE     (1UL << 16)
#define RCC_CFGR_PLLMUL9        (0x7UL << 18)

#define SCB_VTOR (*(volatile uint32_t *)0xE000ED08UL)

uint32_t SystemCoreClock = HSI_VALUE;

void SystemInit(void)
{
    /* Vector table = payload base (must match linker ORIGIN 0x08005900). */
    SCB_VTOR = APP_FLASH_ORIGIN;

    /* Jump-from-BL may still be on PLL. Must switch to HSI and WAIT
     * before gating PLL/HSE — otherwise SYSCLK dies and CPU hangs.
     * Cold-reset path is already HSI so the wait exits immediately. */
    RCC->RCC_CR.REG |= RCC_CR_HSION;
    while ((RCC->RCC_CR.REG & RCC_CR_HSIRDY) == 0U) {
    }

    {
        uint32_t cfgr = RCC->RCC_CFGR.REG;
        cfgr &= ~RCC_CFGR_SW_Msk;
        RCC->RCC_CFGR.REG = cfgr;
    }
    while ((RCC->RCC_CFGR.REG & RCC_CFGR_SWS_Msk) != 0U) {
    }

    RCC->RCC_CR.REG &= ~(RCC_CR_PLLON | RCC_CR_HSEON);
    RCC->RCC_CFGR.REG = 0U;

    SystemCoreClock = HSI_VALUE;
}

status_t SystemClock_DeInit(void)
{
    uint32_t timeout;

    /* Enable HSI */
    RCC->RCC_CR.REG |= (1UL << 0);

    timeout = HSE_STARTUP_TIMEOUT;
    while (((RCC->RCC_CR.REG & (1UL << 1)) == 0U) && (timeout > 0U)) {
        timeout--;
    }

    if (timeout == 0U) {
        return STATUS_ERROR;
    }

    /* Switch SYSCLK back to HSI */
    RCC->RCC_CFGR.BITS.SW = 0U;

    timeout = HSE_STARTUP_TIMEOUT;
    while ((RCC->RCC_CFGR.BITS.SWS != 0U) && (timeout > 0U)) {
        timeout--;
    }

    if (timeout == 0U) {
        return STATUS_ERROR;
    }

    /* Disable PLL */
    RCC->RCC_CR.REG &= ~(1UL << 24);

    timeout = HSE_STARTUP_TIMEOUT;
    while (((RCC->RCC_CR.REG & (1UL << 25)) != 0U) && (timeout > 0U)) {
        timeout--;
    }

    /* Disable HSE */
    RCC->RCC_CR.REG &= ~(1UL << 16);

    /* Restore clock configuration to reset state */
    RCC->RCC_CFGR.REG = 0x00000000UL;

    /* Restore Flash wait states */
    flash_latency_config(0U);

    /* Disable Flash prefetch buffer */
    FLASH->FLASH_ACR.BITS.PRFTBE = 0U;

    /* Update system clock variable */
    SystemCoreClock = HSI_VALUE;

    return STATUS_OK;
}

void SystemCoreClockUpdate(void)
{
    uint32_t sws = RCC->RCC_CFGR.BITS.SWS;
    uint32_t hpre = RCC->RCC_CFGR.BITS.HPRE;
    uint32_t pllmul = RCC->RCC_CFGR.BITS.PLLMUL;
    uint32_t sysclk;
    uint32_t ahb_div;
    static const uint8_t ahb_presc[] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                         1, 2, 3, 4, 6, 7, 8, 9 };

    if (sws == 0U) {
        sysclk = HSI_VALUE;
    } else if (sws == 1U) {
        sysclk = HSE_VALUE;
    } else {
        sysclk = (RCC->RCC_CFGR.BITS.PLLSRC != 0U) ? HSE_VALUE : (HSI_VALUE / 2U);
        sysclk *= (pllmul + 2U);
    }

    ahb_div = ahb_presc[hpre & 0xFU];
    SystemCoreClock = sysclk >> ahb_div;
}

status_t SystemClock_Config(void)
{
    uint32_t timeout;
    uint32_t cfgr;

    flash_latency_config(2U);
    flash_prefetch_enable();

    RCC->RCC_CR.REG |= RCC_CR_HSEON;

    timeout = HSE_STARTUP_TIMEOUT;
    while (((RCC->RCC_CR.REG & RCC_CR_HSERDY) == 0U) && (timeout > 0U)) {
        timeout--;
    }

    if (timeout == 0U) {
        RCC->RCC_CR.REG &= ~RCC_CR_HSEON;
        SystemCoreClock = HSI_VALUE;
        return STATUS_ERROR;
    }

    cfgr = RCC->RCC_CFGR.REG;
    cfgr &= ~(RCC_CFGR_SW_Msk | RCC_CFGR_HPRE_Msk | RCC_CFGR_PPRE1_Msk
            | RCC_CFGR_PPRE2_Msk | RCC_CFGR_PLLSRC_Msk | RCC_CFGR_PLLXTPRE_Msk
            | RCC_CFGR_PLLMUL_Msk);
    cfgr |= RCC_CFGR_PPRE1_DIV2;
    cfgr |= RCC_CFGR_PLLSRC_HSE;
    cfgr |= RCC_CFGR_PLLMUL9;
    RCC->RCC_CFGR.REG = cfgr;

    RCC->RCC_CR.REG |= RCC_CR_PLLON;

    timeout = HSE_STARTUP_TIMEOUT;
    while (((RCC->RCC_CR.REG & RCC_CR_PLLRDY) == 0U) && (timeout > 0U)) {
        timeout--;
    }

    if (timeout == 0U) {
        RCC->RCC_CR.REG &= ~RCC_CR_PLLON;
        SystemCoreClock = HSI_VALUE;
        return STATUS_ERROR;
    }

    cfgr = RCC->RCC_CFGR.REG;
    cfgr &= ~RCC_CFGR_SW_Msk;
    cfgr |= RCC_CFGR_SW_PLL;
    RCC->RCC_CFGR.REG = cfgr;

    timeout = HSE_STARTUP_TIMEOUT;
    while (((RCC->RCC_CFGR.REG & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) && (timeout > 0U)) {
        timeout--;
    }

    if (timeout == 0U) {
        SystemCoreClock = HSI_VALUE;
        return STATUS_ERROR;
    }

    SystemCoreClockUpdate();

    if (tick_init(TICK_INT_PRIORITY) != STATUS_OK) {
        return STATUS_ERROR;
    }

    return STATUS_OK;
}
