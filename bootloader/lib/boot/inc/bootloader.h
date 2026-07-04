/*
 * bootloader.h
 *
 *  Created on: Jun 28, 2026
 *      Author: Minh Tuan
 */

#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include <stdint.h>
#include <img.h>

/* Flash layout (64 KB total)
 *
 * 0x08000000  22 KB   Bootloader
 * 0x08005800  20.5 KB APP1 slot (header + payload)
 * 0x0800AA00  20.5 KB APP2 slot
 * 0x0800FC00   1 KB   OTA config
 */
#define BOOTLOADER_START        0x08000000UL
#define BOOTLOADER_SIZE         0x00005800UL  /* 22 KB */

#define APP1_START              0x08005800UL
#define APP1_SIZE               0x00005200UL  /* 20.5 KB (20992 B) */

#define APP2_START              0x0800AA00UL
#define APP2_SIZE               0x00005200UL  /* 20.5 KB — same as APP1 */

#define OTA_CONFIG_ADDR         0x0800FC00UL  /* last 1 KB page */

#define OTA_MAGIC_REQUEST       0xDEADBEEFUL
#define OTA_MAGIC_NONE          0xFFFFFFFFUL

#define OTA_SLOT_APP1           1U
#define OTA_SLOT_APP2           2U

#define OTA_UPDATE_KEY          'U'

typedef struct {
    uint32_t magic;
    uint32_t active_slot;   /* OTA_SLOT_APP1 or OTA_SLOT_APP2 */
    uint32_t app_size;
    uint32_t crc32;
    uint32_t version;
} ota_config_t;

uint32_t app_get_active_start(void);
uint32_t app_get_inactive_start(void);
uint32_t app_find_boot_addr(void);
int      app_is_valid_at(uint32_t app_addr);
int      app_is_valid(void);
void     jump_to_application(uint32_t app_addr);
int      ota_flag_is_set(void);
int      ota_flag_clear(void);

#endif /* BOOTLOADER_H_ */
