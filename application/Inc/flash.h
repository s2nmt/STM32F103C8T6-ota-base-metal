/*
 * flash.h
 *
 *  Created on: Jun 28, 2026
 *      Author: Minh Tuan
 */

#ifndef FLASH_H_
#define FLASH_H_

#include <stdint.h>

/* STM32F103C8: 64 KB main Flash, 1 KB page */
#define FLASH_MEM_BASE          0x08000000UL
#define FLASH_MEM_SIZE          0x00010000UL
#define FLASH_MEM_END           (FLASH_MEM_BASE + FLASH_MEM_SIZE)
#define FLASH_PAGE_SIZE         0x00000400UL

#define FLASH_KEY1              0x45670123UL
#define FLASH_KEY2              0xCDEF89ABUL

#define FLASH_ACR_LATENCY_Msk   0x00000007UL
#define FLASH_ACR_HLFCYA_Msk    0x00000008UL
#define FLASH_ACR_PRFTBE_Msk    0x00000010UL
#define FLASH_ACR_PRFTBS_Msk    0x00000020UL

typedef struct {
	union {
		unsigned long REG;
		struct {
			unsigned long LATENCY    : 3;
			unsigned long HLFCYA     : 1;
			unsigned long PRFTBE     : 1;
			unsigned long PRFTBS     : 1; /* HW read-only status */
			unsigned long reserved   : 26;
		} BITS;
	} FLASH_ACR;

	unsigned long FLASH_KEYR;
	unsigned long FLASH_OPTKEYR;

	union {
		unsigned long REG;
		struct {
			const unsigned long BSY       : 1;
			unsigned long reserved0       : 1;
			unsigned long PGERR           : 1;
			unsigned long reserved1       : 1;
			unsigned long WRPRTERR        : 1;
			unsigned long EOP             : 1;
			unsigned long reserved2       : 26;
		} BITS;
	} FLASH_SR;

	union {
		unsigned long REG;
		struct {
			unsigned long PG              : 1;
			unsigned long PER             : 1;
			unsigned long MER             : 1;
			unsigned long OPTPG           : 1;
			unsigned long OPTER           : 1;
			unsigned long reserved0       : 1;
			unsigned long STRT            : 1;
			unsigned long LOCK            : 1;
			unsigned long reserved1       : 24;
		} BITS;
	} FLASH_CR;

	unsigned long FLASH_AR;
	unsigned long RESERVED;
	unsigned long FLASH_OBR;
	unsigned long FLASH_WRPR;
} FLASH_TypeDef;

#define FLASH                   ((FLASH_TypeDef *)0x40022000UL)

void flash_prefetch_enable(void);
void flash_latency_config(uint32_t latency);

int  flash_unlock(void);
void flash_lock(void);
int  flash_erase_page(uint32_t page_addr);
int  flash_erase_region(uint32_t start, uint32_t size);
int  flash_write_buffer(uint32_t addr, const uint8_t *data, uint32_t len);
uint16_t flash_read_halfword(uint32_t addr);
uint32_t flash_read_word(uint32_t addr);
#endif /* FLASH_H_ */
