/*
 * uart.h
 *
 *  Created on: Dec 6, 2024
 *      Author: Minh Tuan
 */

#ifndef UART_H_
#define UART_H_

#include <stdint.h>

typedef struct {
	union{
		unsigned long REG;
		struct {
			const unsigned long PE      : 1;
			const unsigned long FE      : 1;
			const unsigned long NE      : 1;
			const unsigned long ORE    	: 1;
			const unsigned long IDLE    : 1;
			unsigned long RXNE      	: 1;
			unsigned long TC      		: 1;
			const unsigned long TXE     : 1;
			unsigned long LBD      		: 1;
			unsigned long CTS           : 1;
			unsigned long reserved      : 22;
		}BITS;
	} USART_SR;

	union{
		unsigned long REG;
		struct {
			unsigned long DR      		: 9;
			unsigned long reserved      : 23;
		}BITS;
	} USART_DR;

	union{
		unsigned long REG;
		struct {
			unsigned long DIV_Fraction    : 4;
			unsigned long DIV_Mantissa    : 12;
			unsigned long reserved        : 16;
		}BITS;
	} USART_BRR;

	union{
		unsigned long REG;
		struct {
			unsigned long SBK   		: 1;
			unsigned long RWU    		: 1;
			unsigned long RE    		: 1;
			unsigned long TE    		: 1;
			unsigned long IDLEIE   		: 1;
			unsigned long RXNEIE    	: 1;
			unsigned long TCIE    		: 1;
			unsigned long TXEIE   		: 1;
			unsigned long PEIE    		: 1;
			unsigned long PS    		: 1;
			unsigned long PCE    		: 1;
			unsigned long WAKE    		: 1;
			unsigned long M    			: 1;
			unsigned long UE    		: 1;
			unsigned long reserved      : 18;
		}BITS;
	} USART_CR1;

	union{
		unsigned long REG;
		struct {
			unsigned long ADD   		: 4;
			unsigned long reserved    	: 1;
			unsigned long LBDL    		: 1;
			unsigned long LBDIE    		: 1;
			unsigned long reserved1   	: 1;
			unsigned long LBCL	    	: 1;
			unsigned long CPHA    		: 1;
			unsigned long CPOL   		: 1;
			unsigned long CLKEN    		: 1;
			unsigned long STOP    		: 2;
			unsigned long LINEN    		: 1;
			unsigned long reserved2     : 17;
		}BITS;
	} USART_CR2;

	union{
		unsigned long REG;
		struct {
			unsigned long EIE   		: 1;
			unsigned long IREN       	: 1;
			unsigned long IRLP    		: 1;
			unsigned long HDSEL    		: 1;
			unsigned long NACK       	: 1;
			unsigned long SCEN	    	: 1;
			unsigned long DMAR    		: 1;
			unsigned long DMAT   		: 1;
			unsigned long RTSE    		: 1;
			unsigned long CTSE    		: 1;
			unsigned long CTSIE    		: 1;
			unsigned long reserved      : 21;
		}BITS;
	} USART_CR3;

	union{
		unsigned long REG;
		struct {
			unsigned long PSC   		: 8;
			unsigned long GT       		: 8;
			unsigned long reserved      : 16;
		}BITS;
	} USART_GTPR;
} USART_TypeDef;

#define USART1    ((USART_TypeDef*)0x40013800)

/* Truy cap register bang REG + mask — KHONG ghi CR1/SR qua bitfield */
#define USART_SR_RXNE       (1UL << 5)
#define USART_SR_TC         (1UL << 6)
#define USART_SR_TXE        (1UL << 7)
#define USART_SR_ORE        (1UL << 3)

#define USART_CR1_RE        (1UL << 2)
#define USART_CR1_TE        (1UL << 3)
#define USART_CR1_RXNEIE    (1UL << 5)
#define USART_CR1_UE        (1UL << 13)

#include <conf.h>

#ifndef UART1_PCLK_HZ
#define UART1_PCLK_HZ       8000000UL
#endif

#define UART_BAUD_9600      9600U
#define UART_BAUD_19200     19200U
#define UART_BAUD_38400     38400U
#define UART_BAUD_57600     57600U
#define UART_BAUD_115200    115200U
#define UART_BAUD_230400    230400U
#define UART_BAUD_460800    460800U
#define UART_BAUD_921600    921600U

void Uart1_Init(void);
void uart1_write(const char *buf, int len);

/* RX ring buffer (USART1 IRQ) */
void uart1_rx_flush(void);
int  uart1_rx_get(uint8_t *out); /* 1 = co byte, 0 = rong */

#endif /* UART_H_ */
