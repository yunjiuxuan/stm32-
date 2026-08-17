#include "reg_led.h"


void LED_REG_Init(void)   // 寄存器方式初始化LED
{
	PUBLIC_RCC_AHB1ENR |= (0x01<<5);
	
	GPIOx_MODER &= ~(0x01<<19);
	GPIOx_MODER |= (0x01<<18);

	GPIOx_OTYPER &= ~(0x01<<9);
	
	GPIOx_OSPEEDR |= (0x01<<19);
	GPIOx_OSPEEDR |= (0x01<<18);
	
	GPIOx_PUPDR &= ~(0x01<<19);
	GPIOx_PUPDR &= ~(0x01<<18);
	
	GPIOx_ODR |= (0x01<<9);
}
