#ifndef _KEY_H
#define _KEY_H

#include <stm32f4xx.h>
 
#define KEY0_PIN     GPIO_Pin_0
#define KEY0_GPIO_Port GPIOA
#define KEY1_PIN     GPIO_Pin_2
#define KEY1_GPIO_Port GPIOE
#define KEY2_PIN     GPIO_Pin_3
#define KEY2_GPIO_Port GPIOE
#define KEY3_PIN     GPIO_Pin_4
#define KEY3_GPIO_Port GPIOE
#define KEY4_PIN     GPIO_Pin_9
#define KEY4_GPIO_Port GPIOA
void key_init();

#endif
