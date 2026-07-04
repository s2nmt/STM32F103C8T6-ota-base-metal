/*
 * gpio.h
 *
 *  Created on: Nov 21, 2024
 *      Author: Minh Tuan
 */

#ifndef GPIO_H_
#define GPIO_H_
#include <stdint.h>
typedef struct
{

	union
	{
		unsigned long REG;
		struct
		{
			unsigned long MODE_0  : 2;
			unsigned long  CNF_0  : 2;
			unsigned long MODE_1  : 2;
			unsigned long  CNF_1  : 2;
			unsigned long MODE_2  : 2;
			unsigned long  CNF_2  : 2;
			unsigned long MODE_3  : 2;
			unsigned long  CNF_3  : 2;
			unsigned long MODE_4  : 2;
			unsigned long  CNF_4  : 2;
			unsigned long MODE_5  : 2;
			unsigned long  CNF_5  : 2;
			unsigned long MODE_6  : 2;
			unsigned long  CNF_6  : 2;
			unsigned long MODE_7  : 2;
			unsigned long  CNF_7  : 2;
		} BITS;

	}CRL;

	union
	{
		unsigned long REG;
		struct
		{
			unsigned long MODE_8  : 2;
			unsigned long  CNF_8  : 2;
			unsigned long MODE_9  : 2;
			unsigned long  CNF_9  : 2;
			unsigned long MODE_10 : 2;
			unsigned long  CNF_10 : 2;
			unsigned long MODE_11 : 2;
			unsigned long  CNF_11 : 2;
			unsigned long MODE_12 : 2;
			unsigned long  CNF_12 : 2;
			unsigned long MODE_13 : 2;
			unsigned long  CNF_13 : 2;
			unsigned long MODE_14 : 2;
			unsigned long  CNF_14 : 2;
			unsigned long MODE_15 : 2;
			unsigned long  CNF_15 : 2;
		} BITS;
	}CRH;


	union
	{
		unsigned long REG;
		struct
		{
			unsigned long b0  : 1;
			unsigned long b1  : 1;
			unsigned long b2  : 1;
			unsigned long b3  : 1;
			unsigned long b4  : 1;
			unsigned long b5  : 1;
			unsigned long b6  : 1;
			unsigned long b7  : 1;
			unsigned long b8  : 1;
			unsigned long b9  : 1;
			unsigned long b10 : 1;
			unsigned long b11 : 1;
			unsigned long b12 : 1;
			unsigned long b13 : 1;
			unsigned long b14 : 1;
			unsigned long b15 : 1;
			unsigned long _reserved : 16;
		} BITS;
	}IDR;


	union
	{
		unsigned long REG;
		struct
		{
			unsigned long b0  : 1;
			unsigned long b1  : 1;
			unsigned long b2  : 1;
			unsigned long b3  : 1;
			unsigned long b4  : 1;
			unsigned long b5  : 1;
			unsigned long b6  : 1;
			unsigned long b7  : 1;
			unsigned long b8  : 1;
			unsigned long b9  : 1;
			unsigned long b10 : 1;
			unsigned long b11 : 1;
			unsigned long b12 : 1;
			unsigned long b13 : 1;
			unsigned long b14 : 1;
			unsigned long b15 : 1;
			unsigned long _reserved : 16;
		} BITS;
	}ODR;

    union
    {
        unsigned long REG;
        struct
        {
            struct
            {
                unsigned short b0  : 1;
                unsigned short b1  : 1;
                unsigned short b2  : 1;
                unsigned short b3  : 1;
                unsigned short b4  : 1;
                unsigned short b5  : 1;
                unsigned short b6  : 1;
                unsigned short b7  : 1;
                unsigned short b8  : 1;
                unsigned short b9  : 1;
                unsigned short b10 : 1;
                unsigned short b11 : 1;
                unsigned short b12 : 1;
                unsigned short b13 : 1;
                unsigned short b14 : 1;
                unsigned short b15 : 1;
            } BSR;

            struct
            {
                unsigned short b0  : 1;
                unsigned short b1  : 1;
                unsigned short b2  : 1;
                unsigned short b3  : 1;
                unsigned short b4  : 1;
                unsigned short b5  : 1;
                unsigned short b6  : 1;
                unsigned short b7  : 1;
                unsigned short b8  : 1;
                unsigned short b9  : 1;
                unsigned short b10 : 1;
                unsigned short b11 : 1;
                unsigned short b12 : 1;
                unsigned short b13 : 1;
                unsigned short b14 : 1;
                unsigned short b15 : 1;
            } BR;
        };
    }BSRR;


	union
	{
		unsigned long REG;
		struct
		{
			unsigned long b0  : 1;
			unsigned long b1  : 1;
			unsigned long b2  : 1;
			unsigned long b3  : 1;
			unsigned long b4  : 1;
			unsigned long b5  : 1;
			unsigned long b6  : 1;
			unsigned long b7  : 1;
			unsigned long b8  : 1;
			unsigned long b9  : 1;
			unsigned long b10 : 1;
			unsigned long b11 : 1;
			unsigned long b12 : 1;
			unsigned long b13 : 1;
			unsigned long b14 : 1;
			unsigned long b15 : 1;
			unsigned long _reserved : 16;
		} BITS;
	}BRR;

	union
	{
		unsigned long REG;
		struct
		{
			unsigned long b0  : 1;
			unsigned long b1  : 1;
			unsigned long b2  : 1;
			unsigned long b3  : 1;
			unsigned long b4  : 1;
			unsigned long b5  : 1;
			unsigned long b6  : 1;
			unsigned long b7  : 1;
			unsigned long b8  : 1;
			unsigned long b9  : 1;
			unsigned long b10 : 1;
			unsigned long b11 : 1;
			unsigned long b12 : 1;
			unsigned long b13 : 1;
			unsigned long b14 : 1;
			unsigned long b15 : 1;
			unsigned long LOCK : 1;
			unsigned long _reserved : 15;
		} BITS;
	}LCKR;

} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *)0x40010800)
#define GPIOB ((GPIO_TypeDef *)0x40010C00)
#define GPIOC ((GPIO_TypeDef *)0x40011000)




typedef enum
{
  GPIO_MODE_INPUT_ANALOG                = (0 << 2) | 0,
  GPIO_MODE_INPUT_FLOAT                 = (1 << 2) | 0,
  GPIO_MODE_INPUT_PULL                  = (2 << 2) | 0,

  GPIO_MODE_OUTPUT_PUSHPULL_10MHz       = (0 << 2) | 1,
  GPIO_MODE_OUTPUT_OPEN_10MHz           = (1 << 2) | 1,
  GPIO_MODE_AF_OUTPUT_PUSHPULL_10MHz    = (2 << 2) | 1,
  GPIO_MODE_AF_OUTPUT_OPEN_10MHz        = (3 << 2) | 1,

  GPIO_MODE_OUTPUT_PUSHPULL_2MHz        = (0 << 2) | 2,
  GPIO_MODE_OUTPUT_OPEN_2MHz            = (1 << 2) | 2,
  GPIO_MODE_AF_OUTPUT_PUSHPULL_2MHz     = (2 << 2) | 2,
  GPIO_MODE_AF_OUTPUT_OPEN_2MHz         = (3 << 2) | 2,

  GPIO_MODE_OUTPUT_PUSHPULL_50MHz       = (0 << 2) | 3,
  GPIO_MODE_OUTPUT_OPEN_50MHz           = (1 << 2) | 3,
  GPIO_MODE_AF_OUTPUT_PUSHPULL_50MHz    = (2 << 2) | 3,
  GPIO_MODE_AF_OUTPUT_OPEN_50MHz        = (3 << 2) | 3,
} GPIO_MODE;

typedef enum
{
	GPIO_RESET = 0,
	GPIO_SET   = 1
} GPIO_STATE;

void GPIO_Mode(volatile GPIO_TypeDef* GPIO, uint8_t GPIO_PIN, GPIO_MODE Mode);
GPIO_STATE GPIO_Read(volatile GPIO_TypeDef* GPIO, uint8_t PIN);
void GPIO_Write(volatile GPIO_TypeDef* GPIO, uint8_t PIN, GPIO_STATE state);
void GPIO_Toggle(volatile GPIO_TypeDef* GPIO, uint8_t PIN);
void GPIO_Init(void);

#endif /* GPIO_H_ */
