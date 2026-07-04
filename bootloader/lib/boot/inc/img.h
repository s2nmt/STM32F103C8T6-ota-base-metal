/*
 * img.h — signed firmware image header (slot layout)
 *
 * Flash slot:
 *   [slot_base + 0]              img_header_t  (IMG_HEADER_SIZE bytes)
 *   [slot_base + payload_offset] app payload   (vector table + code)
 *
 * App linker ORIGIN = slot_base + IMG_HEADER_SIZE (e.g. 0x08005900 for APP1).
 */

#ifndef IMG_H_
#define IMG_H_

#include <stdint.h>

#define IMG_MAGIC               0x494D4731UL   /* "IMG1" */
#define IMG_TYPE_APP            1U
#define IMG_HEADER_SIZE         0x00000100UL   /* 256 B, fixed */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t image_type;
    uint32_t payload_offset;
    uint32_t payload_size;
    uint32_t version;
    uint32_t reserved0;
    uint8_t  sha256[32];
    uint8_t  signature[64];
    uint8_t  reserved1[32];
} img_header_t;

typedef char img_header_size_ok[(sizeof(img_header_t) <= IMG_HEADER_SIZE) ? 1 : -1];

int  img_header_parse(uint32_t slot_base, const img_header_t **out_hdr);
int  img_payload_bounds(uint32_t slot_base, uint32_t slot_size,
                        const img_header_t *hdr,
                        uint32_t *out_addr, uint32_t *out_size);

#endif /* IMG_H_ */
