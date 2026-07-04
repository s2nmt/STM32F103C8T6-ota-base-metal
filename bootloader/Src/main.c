/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Bootloader — OTA UART or verify + jump app
 ******************************************************************************
 */

#include <main.h>
#include <ota.h>
#include <nvic.h>

#define BOOT_WAIT_MS  3000U

int _write(int file, char *ptr, int len)
{
    (void)file;
    uart1_write(ptr, len);
    return len;
}

static int boot_poll_ota_key(void)
{
    uint8_t c;

    while (uart1_rx_get(&c) != 0) {
        if (c == (uint8_t)OTA_UPDATE_KEY) {
            return 1;
        }
    }
    return 0;
}

static int boot_pick_slot(uint32_t *slot_base, uint32_t *slot_size, uint32_t *jump_addr)
{
    uint32_t slots[2];
    uint32_t sizes[2];
    int i;

    slots[0] = app_get_active_start();
    slots[1] = app_get_inactive_start();
    sizes[0] = (slots[0] == APP2_START) ? APP2_SIZE : APP1_SIZE;
    sizes[1] = (slots[1] == APP2_START) ? APP2_SIZE : APP1_SIZE;

    for (i = 0; i < 2; i++) {
        if (boot_verify_slot(slots[i], sizes[i], jump_addr) == 0) {
            *slot_base = slots[i];
            *slot_size = sizes[i];
            return 0;
        }
    }

    return -1;
}

static void boot_enter_ota(void)
{
    printf("OTA RX ready\r\n");
    ota_run(OTA_IDLE_TIMEOUT_MS);
    printf("OTA idle timeout\r\n");
}

static int boot_wait_ota_key(uint32_t timeout_ms)
{
    uint32_t start = tick_get();

    uart1_rx_flush();
    printf("Press '%c' for OTA\r\n", OTA_UPDATE_KEY);

    while ((tick_get() - start) < timeout_ms) {
        if (boot_poll_ota_key()) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    uint32_t slot_base;
    uint32_t slot_size;
    uint32_t jump_addr;

    __asm volatile("cpsie i" : : : "memory");

    init();
    SystemClock_Config();
    GPIO_Init();
    Uart1_Init();
    printf("STM32F103 Bootloader\r\n");

    /* App requested OTA (magic at OTA_CONFIG_ADDR) */
    if (ota_flag_is_set()) {
        printf("OTA flag set\r\n");
        (void)ota_flag_clear();
        boot_enter_ota();
    }

    if (boot_pick_slot(&slot_base, &slot_size, &jump_addr) == 0) {
        if (!boot_wait_ota_key(BOOT_WAIT_MS)) {
            delay_ms(50);
            jump_to_application(jump_addr);
            printf("Jump failed\r\n");
        }

    } else {
        printf("No valid signed app\r\n");
    }

    boot_enter_ota();

    /* Idle timeout: try boot again, else stay in OTA */
    while (1) {
        if (boot_pick_slot(&slot_base, &slot_size, &jump_addr) == 0) {
            delay_ms(50);
            jump_to_application(jump_addr);
        }
        boot_enter_ota();
    }

    return 0;
}
