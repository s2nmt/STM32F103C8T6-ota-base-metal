/*
 * verify.h — boot-time image authentication
 */

#ifndef VERIFY_H_
#define VERIFY_H_

#include <stdint.h>

/* Verify signed slot; on success writes jump address (payload / vector table). */
int boot_verify_slot(uint32_t slot_base, uint32_t slot_size, uint32_t *out_jump_addr);

#endif /* VERIFY_H_ */
