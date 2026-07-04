/*
 * crypto.c
 */

#include <crypto/crypto.h>
#include <crypto/sha256.h>
#include <crypto/uECC.h>
#include <string.h>

void crypto_sha256(const uint8_t *data, uint32_t len, uint8_t out[32])
{
    sha256_ctx_t ctx;

    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

int crypto_memcmp_ct(const uint8_t *a, const uint8_t *b, uint32_t len)
{
    uint8_t diff = 0U;
    uint32_t i;

    for (i = 0U; i < len; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }

    return (int)diff;
}

int crypto_ecdsa_p256_verify(const uint8_t pubkey[64],
                             const uint8_t hash[32],
                             const uint8_t signature[64])
{
    return uECC_verify(pubkey, hash, 32U, signature, uECC_secp256r1());
}
