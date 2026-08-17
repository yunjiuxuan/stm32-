#ifndef _LED_H
#define _LED_H

#include <stm32f4xx.h>

#define LED0_PIN     GPIO_Pin_9
#define LED1_PIN     GPIO_Pin_10
#define LED2_PIN     GPIO_Pin_13
#define LED3_PIN     GPIO_Pin_14

void led_init();

#endif