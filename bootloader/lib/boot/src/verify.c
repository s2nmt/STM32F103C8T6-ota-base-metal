/*
 * verify.c — SHA-256 + ECDSA-P256 gate before jump
 */

#include <verify.h>
#include <img.h>
#include <crypto/crypto.h>
#include <boot_pubkey.h>
#include <bootloader.h>
#include <stdio.h>

int boot_verify_slot(uint32_t slot_base, uint32_t slot_size, uint32_t *out_jump_addr)
{
    const img_header_t *hdr;
    uint8_t computed[32];
    uint32_t payload_addr;
    uint32_t payload_size;
    int rc;

    if (out_jump_addr == NULL) {
        return -1;
    }

    rc = img_header_parse(slot_base, &hdr);
    if (rc != 0) {
        return rc;
    }

    rc = img_payload_bounds(slot_base, slot_size, hdr, &payload_addr, &payload_size);
    if (rc != 0) {
        return rc;
    }

    if (!app_is_valid_at(payload_addr)) {
        return -10;
    }

    crypto_sha256((const uint8_t *)payload_addr, payload_size, computed);

    if (crypto_memcmp_ct(computed, hdr->sha256, 32U) != 0) {
        return -11;
    }

    if (crypto_ecdsa_p256_verify(bootloader_pubkey, computed, hdr->signature) != 1) {
        return -12;
    }

    *out_jump_addr = payload_addr;
    printf("BL: ok %08lX\r\n", (unsigned long)payload_addr);
    return 0;
}
