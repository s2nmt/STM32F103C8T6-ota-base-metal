/*
 * ota_req.c — set OTA flag in flash, then system reset
 */

#include <ota_req.h>
#include <conf.h>
#include <flash.h>

#define APP1_PAYLOAD            0x08005900UL
#define APP2_PAYLOAD            0x0800AB00UL

#define SCB_AIRCR               (*(volatile uint32_t *)0xE000ED0CUL)
#define AIRCR_VECTKEY           0x05FA0000UL
#define AIRCR_SYSRESETREQ       (1UL << 2)

static const ota_config_t *ota_config(void)
{
    return (const ota_config_t *)OTA_CONFIG_ADDR;
}

static void system_reset(void)
{
    __asm volatile("cpsid i" : : : "memory");
    SCB_AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
    while (1) {
    }
}

static uint32_t app_active_slot_guess(void)
{
    if (APP_FLASH_ORIGIN == APP2_PAYLOAD) {
        return OTA_SLOT_APP2;
    }
    return OTA_SLOT_APP1;
}

void ota_request_enter(void)
{
    const ota_config_t *cur = ota_config();
    ota_config_t cfg;

    if (cur->magic != OTA_MAGIC_NONE && cur->magic != OTA_MAGIC_REQUEST) {
        cfg = *cur;
    } else {
        cfg.active_slot = app_active_slot_guess();
        cfg.app_size    = 0U;
        cfg.crc32       = 0U;
        cfg.version     = 0U;
    }

    cfg.magic = OTA_MAGIC_REQUEST;

    if (flash_unlock() != 0) {
        return;
    }

    if (flash_erase_page(OTA_CONFIG_ADDR) != 0) {
        flash_lock();
        return;
    }

    if (flash_write_buffer(OTA_CONFIG_ADDR, (const uint8_t *)&cfg, sizeof(cfg)) != 0) {
        flash_lock();
        return;
    }

    flash_lock();
    system_reset();
}
