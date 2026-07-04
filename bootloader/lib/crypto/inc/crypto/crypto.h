/*
 * crypto.h — SHA-256 + constant-time compare + ECDSA-P256 verify wrapper
 */

#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <stdint.h>

void crypto_sha256(const uint8_t *data, uint32_t len, uint8_t out[32]);
int  crypto_memcmp_ct(const uint8_t *a, const uint8_t *b, uint32_t len);
int  crypto_ecdsa_p256_verify(const uint8_t pubkey[64],
                              const uint8_t hash[32],
                              const uint8_t signature[64]);

#endif /* CRYPTO_H_ */
