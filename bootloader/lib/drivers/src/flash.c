/*
 * flash.c
 *
 *  Created on: Jun 28, 2026
 *      Author: Minh Tuan
 */

#include <flash.h>
#include <stddef.h>

#define FLASH_OP_TIMEOUT        1000000UL

#define FLASH_SR_BSY            (1UL << 0)
#define FLASH_SR_PGERR          (1UL << 2)
#define FLASH_SR_WRPRTERR       (1UL << 4)
#define FLASH_SR_EOP            (1UL << 5)

#define FLASH_CR_PG             (1UL << 0)
#define FLASH_CR_PER            (1UL << 1)
#define FLASH_CR_STRT           (1UL << 6)
#define FLASH_CR_LOCK           (1UL << 7)

void flash_prefetch_enable(void)
{
	uint32_t acr = FLASH->FLASH_ACR.REG;

	acr |= FLASH_ACR_PRFTBE_Msk;
	FLASH->FLASH_ACR.REG = acr;
}

void flash_latency_config(uint32_t latency)
{
	uint32_t acr = FLASH->FLASH_ACR.REG;

	acr &= ~FLASH_ACR_LATENCY_Msk;
	acr |= (latency & FLASH_ACR_LATENCY_Msk);
	FLASH->FLASH_ACR.REG = acr;
}

static void flash_clear_status(void)
{
	/* RM: write 1 to clear PGERR, WRPRTERR, EOP */
	FLASH->FLASH_SR.REG = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;
}

static int flash_wait_ready(void)
{
	uint32_t timeout = FLASH_OP_TIMEOUT;

	/* Dung REG, tranh bitfield RMW sai tren FLASH_CR/SR */
	while (((FLASH->FLASH_SR.REG & FLASH_SR_BSY) != 0U) && (timeout > 0U)) {
		timeout--;
	}

	return (timeout > 0U) ? 0 : -1;
}

static int flash_wait_done(void)
{
	if (flash_wait_ready() != 0) {
		return -1;
	}

	if ((FLASH->FLASH_SR.REG & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0U) {
		flash_clear_status();
		return -1;
	}

	flash_clear_status();
	return 0;
}

static int flash_addr_in_range(uint32_t addr, uint32_t len)
{
	if (len == 0U) {
		return 0;
	}

	if (addr < FLASH_MEM_BASE) {
		return -1;
	}

	if ((addr + len) < addr) {
		return -1;
	}

	if ((addr + len) > FLASH_MEM_END) {
		return -1;
	}

	return 0;
}

static int flash_page_is_protected(uint32_t page_addr)
{
	uint32_t page = (page_addr - FLASH_MEM_BASE) / FLASH_PAGE_SIZE;
	uint32_t wrp_bit = page / 2U;

	if (wrp_bit >= 32U) {
		return 0;
	}

	/* WRPR bit = 0: write protection active on 2-page group */
	if ((FLASH->FLASH_WRPR & (1UL << wrp_bit)) == 0U) {
		return 1;
	}

	return 0;
}

static void flash_clear_op_bits(void)
{
	FLASH->FLASH_CR.REG &= ~(FLASH_CR_PG | FLASH_CR_PER);
}

int flash_unlock(void)
{
	if ((FLASH->FLASH_CR.REG & FLASH_CR_LOCK) != 0U) {
		FLASH->FLASH_KEYR = FLASH_KEY1;
		FLASH->FLASH_KEYR = FLASH_KEY2;
	}

	return ((FLASH->FLASH_CR.REG & FLASH_CR_LOCK) != 0U) ? -1 : 0;
}

void flash_lock(void)
{
	uint32_t timeout = FLASH_OP_TIMEOUT;

	while ((FLASH->FLASH_SR.REG & FLASH_SR_BSY) != 0U) {
		if (--timeout == 0U) {
			break;
		}
	}

	FLASH->FLASH_CR.REG |= FLASH_CR_LOCK;
}

int flash_erase_page(uint32_t page_addr)
{
	if ((page_addr & (FLASH_PAGE_SIZE - 1U)) != 0U) {
		return -1;
	}

	if (flash_addr_in_range(page_addr, FLASH_PAGE_SIZE) != 0) {
		return -1;
	}

	if (flash_page_is_protected(page_addr)) {
		return -1;
	}

	flash_clear_op_bits();

	if (flash_wait_ready() != 0) {
		return -1;
	}

	flash_clear_status();

	/* PER roi STRT bang OR — khong RMW bitfield (STRT write-only) */
	FLASH->FLASH_CR.REG |= FLASH_CR_PER;
	FLASH->FLASH_AR = page_addr;
	FLASH->FLASH_CR.REG |= FLASH_CR_STRT;

	if (flash_wait_done() != 0) {
		FLASH->FLASH_CR.REG &= ~FLASH_CR_PER;
		return -1;
	}

	FLASH->FLASH_CR.REG &= ~FLASH_CR_PER;
	return 0;
}

int flash_erase_region(uint32_t start, uint32_t size)
{
	uint32_t addr;
	uint32_t end;

	if (size == 0U) {
		return 0;
	}

	if (flash_addr_in_range(start, size) != 0) {
		return -1;
	}

	addr = start & ~(FLASH_PAGE_SIZE - 1U);
	end  = start + size;

	while (addr < end) {
		if (flash_erase_page(addr) != 0) {
			return -1;
		}
		addr += FLASH_PAGE_SIZE;
	}

	return 0;
}

int flash_write_buffer(uint32_t addr, const uint8_t *data, uint32_t len)
{
	uint32_t i;

	if (data == NULL) {
		return -1;
	}

	if (len == 0U) {
		return 0;
	}

	if ((addr & 1U) != 0U || (len & 1U) != 0U) {
		return -1;
	}

	if (flash_addr_in_range(addr, len) != 0) {
		return -1;
	}

	for (i = 0U; i < len; i += 2U) {
		uint32_t write_addr = addr + i;
		uint16_t half = (uint16_t)data[i]
		              | ((uint16_t)data[i + 1U] << 8);
		volatile uint16_t *flash_ptr = (volatile uint16_t *)write_addr;

		if (flash_page_is_protected(write_addr & ~(FLASH_PAGE_SIZE - 1U))) {
			return -1;
		}

		flash_clear_op_bits();

		if (flash_wait_ready() != 0) {
			return -1;
		}

		flash_clear_status();

		FLASH->FLASH_CR.REG |= FLASH_CR_PG;
		*flash_ptr = half;

		if (flash_wait_done() != 0) {
			FLASH->FLASH_CR.REG &= ~FLASH_CR_PG;
			return -1;
		}

		FLASH->FLASH_CR.REG &= ~FLASH_CR_PG;

		if (*flash_ptr != half) {
			return -1;
		}
	}

	return 0;
}

uint16_t flash_read_halfword(uint32_t addr)
{
    return *(volatile const uint16_t *)addr;
}

uint32_t flash_read_word(uint32_t addr)
{
    return *(volatile const uint32_t *)addr;
}
