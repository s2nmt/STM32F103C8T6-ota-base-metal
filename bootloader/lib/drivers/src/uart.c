#include <uart.h>
#include <system.h>
#include <rcc.h>
#include <nvic.h>
#include <stddef.h>

#define UART1_RX_BUF_SIZE  512U

static volatile uint8_t  s_rx_buf[UART1_RX_BUF_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;

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
    while ((USART1->USART_SR.REG & USART_SR_TXE) == 0U) {
    }
    USART1->USART_DR.REG = (uint8_t)c;
}

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->USART_SR.REG;

    /* RXNE hoac ORE: doc DR de clear co */
    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0U) {
        uint8_t  b    = (uint8_t)(USART1->USART_DR.REG & 0xFFU);
        uint16_t next = (uint16_t)((s_rx_head + 1U) % UART1_RX_BUF_SIZE);

        if (next != s_rx_tail) {
            s_rx_buf[s_rx_head] = b;
            s_rx_head = next;
        }
    }
}

void uart1_rx_flush(void)
{
    __asm volatile("cpsid i" ::: "memory");
    s_rx_head = 0U;
    s_rx_tail = 0U;
    while ((USART1->USART_SR.REG & (USART_SR_RXNE | USART_SR_ORE)) != 0U) {
        (void)USART1->USART_DR.REG;
    }
    __asm volatile("cpsie i" ::: "memory");
}

int uart1_rx_get(uint8_t *out)
{
    uint16_t tail;

    if (out == NULL) {
        return 0;
    }

    __asm volatile("cpsid i" ::: "memory");
    if (s_rx_head == s_rx_tail) {
        __asm volatile("cpsie i" ::: "memory");
        return 0;
    }
    tail = s_rx_tail;
    *out = s_rx_buf[tail];
    s_rx_tail = (uint16_t)((tail + 1U) % UART1_RX_BUF_SIZE);
    __asm volatile("cpsie i" ::: "memory");
    return 1;
}

void Uart1_Init(void)
{
    enable_RCC_APB2ENR(APB2_USART1);
    USART1->USART_CR1.REG = 0U;
    USART1->USART_CR2.REG = 0U;
    USART1->USART_CR3.REG = 0U;
    USART1->USART_BRR.REG = uart_brr_calc(SystemCoreClock, UART_BAUD_115200);

    s_rx_head = 0U;
    s_rx_tail = 0U;

    /* UE | TE | RE | RXNEIE */
    USART1->USART_CR1.REG = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    nvic_irq_enable(USART1_IRQn);
}

void uart1_write(const char *buf, int len)
{
    for (int i = 0; i < len; i++) {
        uart1_putc(buf[i]);
    }
}
