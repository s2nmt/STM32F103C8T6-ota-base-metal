/*
 * ota_req.h — app requests bootloader OTA via OTA config page
 */

#ifndef OTA_REQ_H_
#define OTA_REQ_H_

#include <stdint.h>

#define OTA_CONFIG_ADDR         0x0800FC00UL
#define OTA_MAGIC_REQUEST       0xDEADBEEFUL
#define OTA_MAGIC_NONE          0xFFFFFFFFUL
#define OTA_SLOT_APP1           1U
#define OTA_SLOT_APP2           2U
#define OTA_UPDATE_KEY          'U'

typedef struct {
    uint32_t magic;
    uint32_t active_slot;
    uint32_t app_size;
    uint32_t crc32;
    uint32_t version;
} ota_config_t;

/* Ghi co OTA va reset MCU -> bootloader vao OTA mode. Khong return. */
void ota_request_enter(void);

#endif /* OTA_REQ_H_ */
