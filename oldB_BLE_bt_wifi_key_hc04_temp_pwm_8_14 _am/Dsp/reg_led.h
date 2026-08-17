#ifndef __REG_LED_H
#define __REG_LED_H


#include "stm32f4xx.h"


#define PUBLIC_RCC_AHB1ENR		(*((volatile uint32_t *)(0x40023800U + 0x30)))

#define GPIOF_BASS				(0x40021400U)
#define GPIOE_BASS				(0x40021000U)
#define GPIOx_BASSD				GPIOF_BASS


#define GPIOx_MODER				(*((volatile uint32_t *)(GPIOx_BASSD + 0x00)))
#define GPIOx_OTYPER			(*((volatile uint32_t *)(GPIOx_BASSD + 0x04)))
#define GPIOx_OSPEEDR			(*((volatile uint32_t *)(GPIOx_BASSD + 0x08)))
#define GPIOx_PUPDR				(*((volatile uint32_t *)(GPIOx_BASSD + 0x0C)))
#define GPIOx_IDR				(*((volatile uint32_t *)(GPIOx_BASSD + 0x10)))
#define GPIOx_ODR				(*((volatile uint32_t *)(GPIOx_BASSD + 0x14)))
#define GPIOx_BSRR				(*((volatile uint32_t *)(GPIOx_BASSD + 0x18)))
#define GPIOx_LCKR				(*((volatile uint32_t *)(GPIOx_BASSD + 0x1C)))
	

#define LED_ON	(GPIOx_ODR &= ~(0x01<<9))
#define LED_OFF	(GPIOx_ODR |=  (0x01<<9))

void LED_REG_Init(void);

#endif

