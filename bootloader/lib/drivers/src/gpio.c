/*
 * gpio.c
 */

#include <gpio.h>
#include <rcc.h>

/*
 * Board pins: PA9/PA10 USART1, PC13 LED.
 */
void GPIO_Init(void)
{
	enable_RCC_APB2ENR(APB2_PORT_A | APB2_PORT_C | APB2_USART1);

	GPIO_Mode(GPIOA, 9, GPIO_MODE_AF_OUTPUT_PUSHPULL_50MHz);
	GPIO_Mode(GPIOA, 10, GPIO_MODE_INPUT_FLOAT);
	GPIO_Mode(GPIOC, 13, GPIO_MODE_OUTPUT_OPEN_50MHz);
}

void GPIO_Mode(volatile GPIO_TypeDef* GPIO, uint8_t GPIO_PIN, GPIO_MODE Mode)
{
  unsigned int reset = ~0, set = 0 ;

  reset &= ~(((1 << 4) - 1) << ((GPIO_PIN % 8)*4));
  set   |= Mode <<  ((GPIO_PIN % 8)*4);
  if(GPIO_PIN > 7){
	  GPIO->CRH.REG &= reset;
	  GPIO->CRH.REG |= set;
  }
  else{
	  GPIO->CRL.REG &= reset;
	  GPIO->CRL.REG |= set;
  }
}

/*
 * Return the IDR register IDR to check Input
 */
GPIO_STATE GPIO_Read(volatile GPIO_TypeDef* GPIO, uint8_t PIN){
	return (GPIO->IDR.REG >> PIN) & 1;
}

/*
 * First I check the current state of the GPIO if it is the same, I will return
 * if it is different, I use the BSRR register to control it.
 */
void GPIO_Write(volatile GPIO_TypeDef* GPIO, uint8_t PIN, GPIO_STATE state){

	if(((GPIO->ODR.REG >> PIN) & 1) == state) return;

	switch(state){
		case GPIO_RESET:
			GPIO->BSRR.REG = (1UL << (16 + PIN));
			break;
		case GPIO_SET:
			GPIO->BSRR.REG = (1UL << PIN);
			break;
	}
}
/*
 * I use the ODR register to detect the current state and then I toggle state
 */
void GPIO_Toggle(volatile GPIO_TypeDef* GPIO, uint8_t PIN){

	GPIO->BSRR.REG = (1UL << (16*((GPIO->ODR.REG >> PIN) & 1) + PIN));
}






