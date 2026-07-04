/*
 * bootloader.c
 *
 * Jump-to-app + OTA slot / flag helpers.
 */

#include <bootloader.h>
#include <flash.h>
#include <system.h>
#include <uart.h>
#include <nvic.h>
#include <stdio.h>

#define NVIC_ICER(i) (*(volatile uint32_t *)(0xE000E180UL + (i) * 4U))
#define NVIC_ICPR(i) (*(volatile uint32_t *)(0xE000E280UL + (i) * 4U))
#define SCB_VTOR     (*(volatile uint32_t *)0xE000ED08UL)
#define SCB_ICSR     (*(volatile uint32_t *)0xE000ED04UL)
#define SCB_ICSR_PENDSTCLR  (1UL << 25)
#define SCB_ICSR_PENDSVCLR  (1UL << 27)

static inline void cpu_disable_irq(void)
{
    __asm volatile("cpsid i" : : : "memory");
}

static inline void cpu_set_msp(uint32_t msp)
{
    __asm volatile("MSR msp, %0" : : "r"(msp) : );
}

static inline void cpu_set_control(uint32_t control)
{
    __asm volatile("MSR control, %0" : : "r"(control) : "memory");
}

static inline void cpu_dsb_isb(void)
{
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
}

static const ota_config_t *ota_config(void)
{
    return (const ota_config_t *)OTA_CONFIG_ADDR;
}

static int slot_bounds_for_payload(uint32_t payload_addr,
                                   uint32_t *slot_base,
                                   uint32_t *slot_end)
{
    if ((payload_addr >= (APP1_START + IMG_HEADER_SIZE)) &&
        (payload_addr < (APP1_START + APP1_SIZE))) {
        *slot_base = APP1_START;
        *slot_end  = APP1_START + APP1_SIZE;
        return 0;
    }

    if ((payload_addr >= (APP2_START + IMG_HEADER_SIZE)) &&
        (payload_addr < (APP2_START + APP2_SIZE))) {
        *slot_base = APP2_START;
        *slot_end  = APP2_START + APP2_SIZE;
        return 0;
    }

    return -1;
}

int app_is_valid_at(uint32_t app_addr)
{
    uint32_t slot_base;
    uint32_t slot_end;
    uint32_t sp;
    uint32_t entry;

    if (slot_bounds_for_payload(app_addr, &slot_base, &slot_end) != 0) {
        return 0;
    }

    (void)slot_base;

    sp    = *(volatile uint32_t *)app_addr;
    entry = *(volatile uint32_t *)(app_addr + 4U);

    if ((sp & 0x2FFE0000UL) != 0x20000000UL) {
        return 0;
    }

    if (entry < app_addr || entry >= slot_end) {
        return 0;
    }

    if ((entry & 1U) == 0U) {
        return 0;
    }

    return 1;
}

uint32_t app_get_active_start(void)
{
    /* Chi nhan APP1/APP2; flash virgin (0xFF..) -> mac dinh APP1 active. */
    if (ota_config()->active_slot == OTA_SLOT_APP2) {
        return APP2_START;
    }
    return APP1_START;
}

uint32_t app_get_inactive_start(void)
{
    /* A/B: active APP1 -> OTA ghi APP2; active APP2 -> OTA ghi APP1. */
    if (app_get_active_start() == APP2_START) {
        return APP1_START;
    }
    return APP2_START;
}

uint32_t app_find_boot_addr(void)
{
    uint32_t active   = app_get_active_start();
    uint32_t inactive = app_get_inactive_start();
    uint32_t active_payload   = active + IMG_HEADER_SIZE;
    uint32_t inactive_payload = inactive + IMG_HEADER_SIZE;

    if (app_is_valid_at(active_payload)) {
        return active_payload;
    }

    if (app_is_valid_at(inactive_payload)) {
        return inactive_payload;
    }

    return 0U;
}

int app_is_valid(void)
{
    return app_find_boot_addr() != 0U;
}

int ota_flag_is_set(void)
{
    return ota_config()->magic == OTA_MAGIC_REQUEST;
}

int ota_flag_clear(void)
{
    const ota_config_t *cur = ota_config();
    ota_config_t cfg;

    if (!ota_flag_is_set()) {
        return 0;
    }

    cfg.magic       = OTA_MAGIC_NONE;
    cfg.active_slot = (cur->active_slot == OTA_SLOT_APP2) ? OTA_SLOT_APP2 : OTA_SLOT_APP1;
    cfg.app_size    = cur->app_size;
    cfg.crc32       = cur->crc32;
    cfg.version     = cur->version;

    if (flash_unlock() != 0) {
        return -1;
    }

    if (flash_erase_page(OTA_CONFIG_ADDR) != 0) {
        flash_lock();
        return -1;
    }

    if (flash_write_buffer(OTA_CONFIG_ADDR, (const uint8_t *)&cfg, sizeof(cfg)) != 0) {
        flash_lock();
        return -1;
    }

    flash_lock();
    return ota_flag_is_set() ? -1 : 0;
}

void jump_to_application(uint32_t app_addr)
{
    uint32_t app_sp    = *(volatile uint32_t *)app_addr;
    uint32_t app_entry = *(volatile uint32_t *)(app_addr + 4U);
    uint32_t i;

    if (!app_is_valid_at(app_addr)) {
        return;
    }

    (void)SystemClock_DeInit();

    USART1->USART_CR1.REG = 0U;
    USART1->USART_CR2.REG = 0U;
    USART1->USART_CR3.REG = 0U;

    SysTick->STK_CTRL.REG = 0U;
    SysTick->STK_LOAD.REG = 0U;
    SysTick->STK_VAL.REG  = 0U;
    SCB_ICSR = SCB_ICSR_PENDSTCLR | SCB_ICSR_PENDSVCLR;

    cpu_disable_irq();

    for (i = 0U; i < 8U; i++) {
        NVIC_ICER(i) = 0xFFFFFFFFUL;
        NVIC_ICPR(i) = 0xFFFFFFFFUL;
    }

    SCB_VTOR = app_addr;
    cpu_dsb_isb();

    cpu_set_msp(app_sp);
    cpu_set_control(0U);
    cpu_dsb_isb();

    __asm volatile("bx %0" : : "r"(app_entry) : );

    while (1) {
    }
}
