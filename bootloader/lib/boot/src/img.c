/*
 * img.c — parse signed image header
 */

#include <img.h>
#include <stddef.h>

int img_header_parse(uint32_t slot_base, const img_header_t **out_hdr)
{
    const img_header_t *hdr = (const img_header_t *)slot_base;

    if (out_hdr == NULL) {
        return -1;
    }

    if (hdr->magic != IMG_MAGIC) {
        return -1;
    }

    if (hdr->image_type != IMG_TYPE_APP) {
        return -2;
    }

    if (hdr->payload_offset < IMG_HEADER_SIZE) {
        return -3;
    }

    if (hdr->payload_size == 0U) {
        return -4;
    }

    *out_hdr = hdr;
    return 0;
}

int img_payload_bounds(uint32_t slot_base, uint32_t slot_size,
                       const img_header_t *hdr,
                       uint32_t *out_addr, uint32_t *out_size)
{
    uint32_t payload_addr;
    uint32_t payload_end;

    if (hdr == NULL || out_addr == NULL || out_size == NULL) {
        return -1;
    }

    payload_addr = slot_base + hdr->payload_offset;
    payload_end  = payload_addr + hdr->payload_size;

    if (payload_end < payload_addr) {
        return -2;
    }

    if (payload_end > (slot_base + slot_size)) {
        return -3;
    }

    *out_addr = payload_addr;
    *out_size = hdr->payload_size;
    return 0;
}
