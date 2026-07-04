/*
 * rcc.h
 *
 *  Created on: Nov 26, 2024
 *      Author: Minh Tuan
 */

#ifndef RCC_H_
#define RCC_H_

typedef struct
{
	union{
		unsigned long REG;
		struct {
			unsigned long HSION       		: 1;  // Internal high-speed clock enable
			const unsigned long HSIRDY 		: 1;  // Internal high-speed clock ready flag. use const variabl to not allow write this bit
			unsigned long _revert_1   	 	: 1;
			unsigned long HSITRIM      		: 5;  // Internal high-speed clock trimming
			const unsigned long HSICAL      : 8;  // Internal high-spped clock calibration
			unsigned long HSEON        		: 1;  // HSE clock enable
			const unsigned long HSERDY	    : 1;  // External high-speed clock ready flag
			unsigned long HSEBYP       		: 1;  // External high-speed clock bypass
			unsigned long CSSON        		: 1;
			unsigned long _revert_2    		: 4;
			unsigned long PLLON   	   		: 1;
			unsigned long PLL_RDY      		: 1;
			unsigned long PLL2_ON      		: 1;
			const unsigned long PLL2_RDY  	: 1;
			unsigned long PLL3_ON      		: 1;
			const unsigned long PLL3_RDY    : 1;
			unsigned long _revert_3    		: 2;
		} BITS;
	} RCC_CR;

	union{
		unsigned long REG;
		struct {
			unsigned long SW 		        : 2;
			const unsigned long SWS 		: 2;
			unsigned long HPRE				: 4;
			unsigned long PPRE1				: 3;
			unsigned long PPRE2				: 3;
			unsigned long ADCPRE 			: 2;
			unsigned long PLLSRC			: 1;
			unsigned long PLLXTPRE			: 1;
			unsigned long PLLMUL			: 4;
			unsigned long OTGFSPRE			: 1;
			unsigned long _revert_1			: 1;
			unsigned long MCO   			: 4;
			unsigned long _revert_2			: 4;
		} BITS;
	} RCC_CFGR;

	union{
		unsigned long REG;
		struct {
			const unsigned long SIRDYF 		: 1;
			const unsigned long LSERDYF 	: 1;
			const unsigned long HSIRDYF		: 1;
			const unsigned long HSERDYF		: 1;
			const unsigned long PLLRDYF		: 1;
			const unsigned long PLL2RDYF 	: 1;
			const unsigned long PLL3RDY		: 1;
			const unsigned long CSSF  		: 1;
			unsigned long LSIRDYIE			: 1;
			unsigned long LSERDYIE			: 1;
			unsigned long HSIRDYIE			: 1;
			unsigned long HSERDYIE   		: 1;
			unsigned long PLLRDYIE			: 1;
			unsigned long PLL2RDYIE			: 1;
			unsigned long PLL3RDYIE			: 1;
			unsigned long _revert_1			: 1;
			unsigned long LSIRDYC			: 1;
			unsigned long LSERDYC			: 1;
			unsigned long HSIRDYC			: 1;
			unsigned long HSERDYC			: 1;
			unsigned long PLLRDYC			: 1;
			unsigned long PLL2RDYC			: 1;
			unsigned long PLL3RDYC			: 1;
			unsigned long CSSC  			: 1;
			unsigned long _revert_2			: 8;
		} BITS;
	} RCC_CIR;

	union{
		unsigned long REG;
		struct {
			unsigned long AFIORST 		    : 1;
			unsigned long _revert_1			: 1;
			unsigned long IOPARST		    : 1;
			unsigned long IOPBRST			: 1;
			unsigned long IOPCRST			: 1;
			unsigned long IOPDRST 			: 1;
			unsigned long IOPERST			: 1;
			unsigned long _revert_2  		: 2;
			unsigned long ADC1RST			: 1;
			unsigned long ADC2RST			: 1;
			unsigned long TIM1RST			: 1;
			unsigned long SPI1RST   		: 1;
			unsigned long _revert_3			: 1;
			unsigned long USART1RST			: 1;
			unsigned long _revert_4			: 17;
		} BITS;
	} RCC_APB2RSTR;

	union{
		unsigned long REG;
		struct {
			unsigned long TIM2RST 		    : 1;
			unsigned long TIM3RST			: 1;
			unsigned long TIM4RST		    : 1;
			unsigned long TIM5RST			: 1;
			unsigned long TIM6RST			: 1;
			unsigned long TIM7RST 			: 1;
			unsigned long _revert_1			: 5;
			unsigned long WWDGRST  	    	: 1;
			unsigned long _revert_2			: 2;
			unsigned long SPI2RST			: 1;
			unsigned long SPI3RST			: 1;
			unsigned long _revert_3   		: 1;
			unsigned long USART2RST			: 1;
			unsigned long USART3RST			: 1;
			unsigned long USART4RST			: 1;
			unsigned long USART5RST			: 1;
			unsigned long I2C1RST			: 1;
			unsigned long I2C2RST			: 1;
			unsigned long _revert_4   		: 2;
			unsigned long CAN1RST			: 1;
			unsigned long CAN2RST			: 1;
			unsigned long BKPRST			: 1;
			unsigned long PWRRST			: 1;
			unsigned long DACRST			: 1;
			unsigned long _revert_5   		: 2;
		} BITS;
	} RCC_APB1RSTR;

	union{
		unsigned long REG;
		struct {
			unsigned long DMA1EN 		    : 1;
			unsigned long DMA2EN			: 1;
			unsigned long SRAMEN		    : 1;
			unsigned long _revert_1			: 1;
			unsigned long FLITFEN			: 1;
			unsigned long _revert_2			: 1;
			unsigned long CRCEN				: 1;
			unsigned long _revert_3  	    : 5;
			unsigned long OTGFSEN			: 1;
			unsigned long _revert_4			: 1;
			unsigned long ETHMACEN			: 1;
			unsigned long ETHMACTXEN   		: 1;
			unsigned long ETHMACRXEN		: 1;
			unsigned long _revert_5   		: 15;
		} BITS;
	} RCC_AHBENR;

	union{
		unsigned long REG;
		struct {
			unsigned long AFIOEN		    : 1;
			unsigned long _revert_1			: 1;
			unsigned long IOPAEN		    : 1;
			unsigned long IOPBEN			: 1;
			unsigned long IOPCEN			: 1;
			unsigned long IOPDEN			: 1;
			unsigned long IOPEEN			: 1;
			unsigned long _revert_2  		: 2;
			unsigned long ADC1EN			: 1;
			unsigned long ADC2EN			: 1;
			unsigned long TIM1EN			: 1;
			unsigned long SPI1EN   		    : 1;
			unsigned long _revert_3			: 1;
			unsigned long USART1EN			: 1;
			unsigned long _revert_4			: 17;
		} BITS;
	} RCC_APB2ENR;

	union{
		unsigned long REG;
		struct {
			unsigned long TIM2EN 		    : 1;
			unsigned long TIM3EN			: 1;
			unsigned long TIM4EN		    : 1;
			unsigned long TIM5EN			: 1;
			unsigned long TIM6EN			: 1;
			unsigned long TIM7EN			: 1;
			unsigned long _revert_1			: 5;
			unsigned long WWDGEN  	    	: 1;
			unsigned long _revert_2			: 2;
			unsigned long SPI2EN			: 1;
			unsigned long SPI3EN			: 1;
			unsigned long _revert_3   		: 1;
			unsigned long USART2EN			: 1;
			unsigned long USART3EN			: 1;
			unsigned long USART4EN			: 1;
			unsigned long USART5EN			: 1;
			unsigned long I2C1EN			: 1;
			unsigned long I2C2EN			: 1;
			unsigned long _revert_4   		: 2;
			unsigned long CAN1EN			: 1;
			unsigned long CAN2EN			: 1;
			unsigned long BKPEN				: 1;
			unsigned long PWREN				: 1;
			unsigned long DACEN				: 1;
			unsigned long _revert_5   		: 2;
		} BITS;
	} RCC_APB1ENR;

	union{
		unsigned long REG;
		struct{
			unsigned long LSEON 	 		: 1;
			const unsigned long LSERDY 	 	: 1;
			unsigned long LSEBYP 	 		: 1;
			unsigned long _revert_1			: 5;
			unsigned long RTCSEL 	 		: 2;
			unsigned long _revert_2			: 5;
			unsigned long RTCEN  	 		: 1;
			unsigned long BDRST  	 		: 1;
			unsigned long _revert_3			: 15;
		};
	} RCC_BDCR;

	union{
		unsigned long REG;
		struct{
			unsigned long LSION 	 		: 1;
			const unsigned long LSIRDY 	 	: 1;
			unsigned long _revert_1			: 22;
			unsigned long RMVF 	 			: 1;
			unsigned long _revert_2			: 1;
			unsigned long PINRSTF  	 		: 1;
			unsigned long PORRSTF  	 		: 1;
			unsigned long SFTRSTF			: 1;
			unsigned long IWDGRSTF			: 1;
			unsigned long WWDGRSTF			: 1;
			unsigned long LPWRRSTF			: 1;
		};
	} RCC_CSR;

	union{
		unsigned long REG;
		struct{
			unsigned long _revert_1			: 12;
			unsigned long OTGFSRst 	 		: 1;
			unsigned long _revert_2			: 1;
			unsigned long ETHMACRST  	 	: 1;
			unsigned long _revert_3  	 	: 17;
		};
	} RCC_AHBRSTR;

	union{
		unsigned long REG;
		struct{
			unsigned long PREDIV1			: 4;
			unsigned long PREDIV2 	 		: 4;
			unsigned long PLL2MUL			: 4;
			unsigned long PLL3MUL   	 	: 4;
			unsigned long PREDIV1SRC  	 	: 1;
			unsigned long I2S2SRC   	 	: 1;
			unsigned long I2S3SRC   	 	: 1;
			unsigned long _revert_1			: 13;
		};
	} RCC_CFGR2;
} RCC_TypeDef;

typedef enum {
	APB2_AFIO   = 1 << 0,
	APB2_PORT_A = 1 << 2,
	APB2_PORT_B = 1 << 3,
	APB2_PORT_C = 1 << 4,
	APB2_PORT_D = 1 << 5,
	APB2_PORT_E = 1 << 6,
	APB2_ADC1   = 1 << 9,
	APB2_ADC2   = 1 << 10,
	APB2_TIM1   = 1 << 11,
	APB2_SPI1   = 1 << 12,
	APB2_USART1 = 1 << 14
} APB2EN_REG_TypeDef;

#define RCC ((RCC_TypeDef *)0x40021000)

#define RCC_CR_HSION  0x00000001UL
#define RCC_CR_HSIRDY 0x00000002UL
#define RCC_CR_HSEON  0x00010000UL
#define RCC_CR_HSERDY 0x00020000UL
#define RCC_CR_HSEBYP 0x00040000UL
#define RCC_CR_CSSON  0x00080000UL
#define RCC_CR_PLLON  0x01000000UL
#define RCC_CR_PLLRDY 0x02000000UL


void enable_RCC_APB2ENR(unsigned long REG);

#endif /* RCC_H_ */
