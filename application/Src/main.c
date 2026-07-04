/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : App1 — blink LED + UART; 'U' -> OTA reset
 ******************************************************************************
 */

#include <main.h>
#include <nvic.h>
#include <ota_req.h>

static volatile uint8_t s_rx_buf[64];
static volatile uint8_t s_rx_count;

int _write(int file, char *ptr, int len)
{
    (void)file;
    uart1_write(ptr, len);
    return len;
}

void USART1_IRQHandler(void)
{
    if (USART1->USART_SR.BITS.RXNE) {
        char c = (char)USART1->USART_DR.REG;
        if (s_rx_count < sizeof(s_rx_buf)) {
            s_rx_buf[s_rx_count++] = (uint8_t)c;
        }
    }
}

static int app_take_rx_byte(uint8_t *out)
{
    if (s_rx_count == 0U) {
        return 0;
    }

    *out = s_rx_buf[0];
    s_rx_count--;

    for (uint8_t i = 0U; i < s_rx_count; i++) {
        s_rx_buf[i] = s_rx_buf[i + 1U];
    }

    return 1;
}

static void app_poll_ota_key(void)
{
    uint8_t c;

    while (app_take_rx_byte(&c)) {
        if (c == (uint8_t)OTA_UPDATE_KEY) {
            printf("OTA requested, resetting...\r\n");
            delay_ms(50);
            ota_request_enter();
        }
    }
}

int main(void)
{
    __asm volatile("cpsie i" : : : "memory");

    init();
    SystemClock_Config();
    GPIO_Init();
    Uart1_Init();

    printf("app 2 started\r\n");
    printf("Press '%c' for OTA update\r\n", OTA_UPDATE_KEY);

    Iwdg_Init();

    while (1) {
        app_poll_ota_key();
        printf("[%lu] Hello world\r\n", tick_get());
        Iwdg_Feed();
        GPIO_Toggle(GPIOC, 13);
        delay_ms(1000);
    }
}
