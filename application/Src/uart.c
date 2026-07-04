#include <uart.h>
#include <nvic.h>
#include <system.h>
#include <rcc.h>
static uint16_t uart_brr_calc(uint32_t pclk_hz, uint32_t baud)
{
    uint32_t divider  = (25U * pclk_hz) / (4U * baud);
    uint32_t mantissa = divider / 100U;
    uint32_t fraction = (((divider - mantissa * 100U) * 16U) + 50U) / 100U;

    if (fraction >= 16U) {
        fraction = 0U;
        mantissa++;
    }

    return (uint16_t)((mantissa << 4) | (fraction & 0xFU));
}

static void uart1_putc(char c)
{
    while (!USART1->USART_SR.BITS.TXE) {
    }
    USART1->USART_DR.REG = (uint8_t)c;
}

void Uart1_Init()
{
    enable_RCC_APB2ENR(APB2_USART1);
    USART1->USART_CR1.REG = 0U;
    USART1->USART_CR2.REG = 0U;
    USART1->USART_CR3.REG = 0U;
    USART1->USART_BRR.REG = uart_brr_calc(SystemCoreClock, UART_BAUD_115200);
    USART1->USART_CR1.REG = (1U << 13) | (1U << 3) | (1U << 2) | (1U << 5);

    nvic_irq_enable(USART1_IRQn);
}

void uart1_write(const char *buf, int len)
{
    for (int i = 0; i < len; i++) {
        uart1_putc(buf[i]);
    }
}
