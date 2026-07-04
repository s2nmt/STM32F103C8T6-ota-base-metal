/*
 * ota.c
 *
 *  Created on: Jun 28, 2026
 *      Author: Minh Tuan
 */

#include <ota.h>
#include <bootloader.h>
#include <flash.h>
#include <img.h>
#include <uart.h>
#include <verify.h>
#include <stdio.h>
#include <string.h>

#include <tick.h>
#include <iwdg.h>

#define OTA_RX_TIMEOUT  (-2)

typedef enum {
    RX_WAIT_STX,
    RX_WAIT_CMD,
    RX_WAIT_LEN_L,
    RX_WAIT_LEN_H,
    RX_WAIT_PAYLOAD,
    RX_WAIT_CRC_L,
    RX_WAIT_CRC_H,
    RX_WAIT_ETX
} rx_state_t;

static uint8_t    rx_payload[OTA_MAX_PAYLOAD];
static uint16_t   rx_len;
static uint16_t   rx_index;
static uint8_t    rx_cmd;
static uint16_t   rx_crc_recv;
static rx_state_t rx_state;

static uint32_t fw_size;
static uint32_t fw_crc32;
static uint32_t fw_written;
static uint16_t fw_seq_expected;
static uint32_t fw_write_addr;
static uint32_t fw_slot_size;
static uint8_t  fw_slot_checked; /* da kiem tra vector/link dung slot */
static uint8_t  fw_aborted;      /* reject som — NACK moi frame sau do */

static uint32_t s_idle_limit;

/* Khi da co header + vector table: tu choi image link sai slot (truoc khi flash het). */
static int ota_early_slot_check(void)
{
    const img_header_t *hdr;
    uint32_t payload_addr;
    uint32_t payload_size;
    uint32_t need;

    if (fw_slot_checked != 0U) {
        return 0;
    }

    if (fw_written < sizeof(img_header_t)) {
        return 0;
    }

    if (img_header_parse(fw_write_addr, &hdr) != 0) {
        printf("OTA: bad hdr\r\n");
        return -1;
    }

    if (img_payload_bounds(fw_write_addr, fw_slot_size, hdr,
                           &payload_addr, &payload_size) != 0) {
        printf("OTA: bad bounds\r\n");
        return -1;
    }

    /* Can SP + Reset (8 B) tai payload */
    need = (payload_addr - fw_write_addr) + 8U;
    if (fw_written < need) {
        return 0;
    }

    if (payload_addr != (fw_write_addr + IMG_HEADER_SIZE)) {
        printf("OTA: bad offset\r\n");
        return -1;
    }

    /* Reset/SP phai nam trong slot dang ghi — app1 link vao APP2 se fail o day */
    if (!app_is_valid_at(payload_addr)) {
        printf("OTA: wrong slot img\r\n");
        return -1;
    }

    fw_slot_checked = 1U;
    return 0;
}

static void ota_tx_byte(uint8_t b)
{
    while ((USART1->USART_SR.REG & USART_SR_TXE) == 0U) {
    }
    USART1->USART_DR.REG = b;
}

#if OTA_DEBUG
static void ota_dbg_rx(uint8_t b)
{
    printf("RX:%02X ", (unsigned)b);
}

static void ota_dbg_frame_ok(void)
{
    uint32_t i;
    printf("\r\nDBG frame cmd=0x%02X len=%u data=",
           (unsigned)rx_cmd, (unsigned)rx_len);
    for (i = 0U; i < rx_len; i++) {
        printf("%02X ", (unsigned)rx_payload[i]);
    }
    printf("crc=%04X\r\n", (unsigned)rx_crc_recv);
}
#else
static void ota_dbg_rx(uint8_t b)
{
    (void)b;
}

static void ota_dbg_frame_ok(void)
{
}
#endif

static void ota_tx_ack(void)
{
    ota_tx_byte(OTA_RSP_ACK);
    /* Cho byte ra het day TX truoc khi jump (tat UART). */
    while ((USART1->USART_SR.REG & USART_SR_TC) == 0U) {
    }
}

static void ota_tx_nack(void)
{
    ota_tx_byte(OTA_RSP_NACK);
    while ((USART1->USART_SR.REG & USART_SR_TC) == 0U) {
    }
}

static int uart_rx_byte(uint8_t *out)
{
    uint32_t deadline = tick_get() + s_idle_limit;

    /* Doc tu ring buffer (IRQ nap). Idle chi reset khi STX / dang trong frame. */
    while ((int32_t)(tick_get() - deadline) < 0) {
        if (uart1_rx_get(out) != 0) {
            ota_dbg_rx(*out);
            if (rx_state != RX_WAIT_STX || *out == OTA_FRAME_STX) {
                deadline = tick_get() + s_idle_limit;
            }
            return 0;
        }
        Iwdg_Feed();
    }
    return OTA_RX_TIMEOUT;
}

static uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000U) {
            crc = (uint16_t)((crc << 1) ^ 0x1021U);
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static uint16_t crc16_calc(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        crc = crc16_update(crc, data[i]);
    }
    return crc;
}

static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1UL) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

static uint32_t crc32_flash(uint32_t addr, uint32_t len)
{
    return crc32_calc((const uint8_t *)addr, len);
}

static uint32_t ota_slot_size(uint32_t addr)
{
    if (addr == APP2_START) {
        return APP2_SIZE;
    }
    return APP1_SIZE;
}

static void ota_reset_rx(void)
{
    rx_state = RX_WAIT_STX;
    rx_len   = 0U;
    rx_index = 0U;
}

static int ota_receive_frame(void)
{
    uint8_t byte;

    if (uart_rx_byte(&byte) != 0) {
        return OTA_RX_TIMEOUT;
    }

    switch (rx_state) {
    case RX_WAIT_STX:
        if (byte == OTA_FRAME_STX) {
            rx_state = RX_WAIT_CMD;
        }
        break;

    case RX_WAIT_CMD:
        rx_cmd   = byte;
        rx_state = RX_WAIT_LEN_L;
        break;

    case RX_WAIT_LEN_L:
        rx_len   = byte;
        rx_state = RX_WAIT_LEN_H;
        break;

    case RX_WAIT_LEN_H:
        rx_len  |= (uint16_t)byte << 8;
        rx_index = 0U;
        if (rx_len > OTA_MAX_PAYLOAD) {
            ota_reset_rx();
            return -1;
        }
        rx_state = (rx_len == 0U) ? RX_WAIT_CRC_L : RX_WAIT_PAYLOAD;
        break;

    case RX_WAIT_PAYLOAD:
        rx_payload[rx_index++] = byte;
        if (rx_index >= rx_len) {
            rx_state = RX_WAIT_CRC_L;
        }
        break;

    case RX_WAIT_CRC_L:
        rx_crc_recv  = byte;
        rx_state = RX_WAIT_CRC_H;
        break;

    case RX_WAIT_CRC_H:
        rx_crc_recv |= (uint16_t)byte << 8;
        rx_state     = RX_WAIT_ETX;
        break;

    case RX_WAIT_ETX:
        if (byte != OTA_FRAME_ETX) {
            ota_reset_rx();
            return -1;
        }
        {
            uint8_t  body[3U + OTA_MAX_PAYLOAD];
            uint16_t blen = (uint16_t)(3U + rx_len);
            uint16_t calc;

            /* Giu rx_cmd/rx_len/rx_payload cho ota_process_frame(); reset sau. */
            body[0] = rx_cmd;
            body[1] = (uint8_t)(rx_len & 0xFFU);
            body[2] = (uint8_t)(rx_len >> 8);
            if (rx_len > 0U) {
                memcpy(body + 3U, rx_payload, rx_len);
            }
            calc = crc16_calc(body, blen);
            if (calc != rx_crc_recv) {
#if OTA_DEBUG
                printf("\r\nDBG CRC fail calc=%04X recv=%04X\r\n",
                       (unsigned)calc, (unsigned)rx_crc_recv);
#endif
                ota_reset_rx();
                return -1;
            }
            ota_dbg_frame_ok();
            rx_state = RX_WAIT_STX;
        }
        return 0;

    default:
        ota_reset_rx();
        break;
    }

    return 1;
}

static int ota_handle_start(void)
{
    if (rx_len != 8U) {
        return -1;
    }

    fw_size   = (uint32_t)rx_payload[0]
              | ((uint32_t)rx_payload[1] << 8)
              | ((uint32_t)rx_payload[2] << 16)
              | ((uint32_t)rx_payload[3] << 24);
    fw_crc32  = (uint32_t)rx_payload[4]
              | ((uint32_t)rx_payload[5] << 8)
              | ((uint32_t)rx_payload[6] << 16)
              | ((uint32_t)rx_payload[7] << 24);

    {
        uint32_t active = app_get_active_start();
        uint32_t inactive = app_get_inactive_start();

        /* Chi cho phep OTA vao slot inactive — khong ghi de app dang active. */
        if ((inactive != APP1_START && inactive != APP2_START) ||
            (inactive == active)) {
            printf("OTA: bad slot\r\n");
            return -1;
        }

        fw_write_addr   = inactive;
        fw_slot_size    = ota_slot_size(fw_write_addr);
        fw_written      = 0U;
        fw_seq_expected = 0U;
        fw_slot_checked = 0U;
        fw_aborted      = 0U;

        printf("OTA: act=%08lX inact=%08lX\r\n",
               (unsigned long)active, (unsigned long)inactive);
    }

    if (fw_size == 0U || fw_size > fw_slot_size) {
        printf("OTA: bad size\r\n");
        return -1;
    }

    printf("OTA: %lu B -> %08lX\r\n",
           (unsigned long)fw_size, (unsigned long)fw_write_addr);

    if (flash_unlock() != 0) {
        printf("OTA: unlock fail\r\n");
        return -1;
    }

    /* Chi erase dung so page chua image (nhanh hon erase ca slot). */
    {
        uint32_t erase_size = (fw_size + FLASH_PAGE_SIZE - 1U) & ~(FLASH_PAGE_SIZE - 1U);

        if (erase_size > fw_slot_size) {
            erase_size = fw_slot_size;
        }

        if (flash_erase_region(fw_write_addr, erase_size) != 0) {
            printf("OTA: erase fail\r\n");
            flash_lock();
            return -1;
        }
    }

    Iwdg_Feed();
    return 0;
}

static int ota_handle_data(void)
{
    uint16_t seq;
    uint32_t chunk_len;
    uint8_t  pad[OTA_CHUNK_SIZE + 2U];

    if (fw_aborted != 0U) {
        return -1;
    }

    if (rx_len < 2U) {
        return -1;
    }

    seq = (uint16_t)rx_payload[0] | ((uint16_t)rx_payload[1] << 8);
    if (seq != fw_seq_expected) {
        return -1;
    }

    chunk_len = (uint32_t)rx_len - 2U;
    if ((fw_written + chunk_len) > fw_size) {
        return -1;
    }

    if (chunk_len == 0U) {
        return -1;
    }

    memcpy(pad, rx_payload + 2U, chunk_len);
    if (chunk_len & 1U) {
        pad[chunk_len++] = 0xFFU;
    }

    if (flash_write_buffer(fw_write_addr + fw_written, pad, chunk_len) != 0) {
        return -1;
    }

    Iwdg_Feed();
    fw_written     += (uint32_t)rx_len - 2U;
    fw_seq_expected = (uint16_t)(fw_seq_expected + 1U);

    /* Sau ~264 B (header+vector): bat image link sai slot, khong cho flash het. */
    if (ota_early_slot_check() != 0) {
        fw_aborted = 1U;
        flash_lock();
        return -1;
    }

    return 0;
}

static int ota_handle_end(void)
{
    uint32_t calc;
    ota_config_t cfg;
    uint32_t new_slot;

    if (fw_written != fw_size) {
        printf("OTA: size mismatch\r\n");
        return -1;
    }

    calc = crc32_flash(fw_write_addr, fw_size);
    if (calc != fw_crc32) {
        printf("OTA: CRC fail\r\n");
        flash_lock();
        return -1;
    }

    /* Chi commit neu image dung slot inactive (vector/link dung dia chi). */
    {
        uint32_t jump_addr;
        uint32_t slot_size = (fw_write_addr == APP2_START) ? APP2_SIZE : APP1_SIZE;
        uint32_t active    = app_get_active_start();
        uint32_t expect_payload = fw_write_addr + IMG_HEADER_SIZE;

        if (fw_write_addr == active) {
            printf("OTA: active slot\r\n");
            flash_lock();
            return -1;
        }

        if (boot_verify_slot(fw_write_addr, slot_size, &jump_addr) != 0) {
            printf("OTA: verify fail\r\n");
            flash_lock();
            return -1;
        }

        if (jump_addr != expect_payload) {
            printf("OTA: bad link\r\n");
            flash_lock();
            return -1;
        }

        new_slot = (fw_write_addr == APP2_START) ? OTA_SLOT_APP2 : OTA_SLOT_APP1;

        cfg.magic       = OTA_MAGIC_NONE;
        cfg.active_slot = new_slot;
        cfg.app_size    = fw_size;
        cfg.crc32       = fw_crc32;
        cfg.version     = 0U;

        if (flash_erase_page(OTA_CONFIG_ADDR) != 0) {
            flash_lock();
            return -1;
        }

        if (flash_write_buffer(OTA_CONFIG_ADDR, (const uint8_t *)&cfg, sizeof(cfg)) != 0) {
            flash_lock();
            return -1;
        }

        flash_lock();

        /* PC tool match "OTA OK" */
        printf("OTA OK %08lX\r\n", (unsigned long)jump_addr);
        ota_tx_ack();
        jump_to_application(jump_addr);
    }
    return 0;
}

static void ota_process_frame(void)
{
    int rc = -1;

    switch (rx_cmd) {
    case OTA_CMD_START:
        rc = ota_handle_start();
        break;
    case OTA_CMD_DATA:
        rc = ota_handle_data();
        break;
    case OTA_CMD_END:
        /* Thanh cong: handle_end tu ACK + jump, khong return. */
        rc = ota_handle_end();
        break;
    default:
        break;
    }

    if (rc == 0) {
        ota_tx_ack();
    } else {
        ota_tx_nack();
    }
}

void ota_run(uint32_t idle_timeout_ms)
{
    s_idle_limit = idle_timeout_ms;
    uart1_rx_flush();
    ota_reset_rx();

    while (1) {
        int st = ota_receive_frame();

        if (st == OTA_RX_TIMEOUT) {
            return;
        }

        if (st == 0) {
            ota_process_frame();
            ota_reset_rx();
        } else if (st < 0) {
            ota_tx_nack();
        }
    }
}
