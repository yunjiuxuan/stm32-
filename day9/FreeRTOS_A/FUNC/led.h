#ifndef _LED_H
#define _LED_H

#include <stm32f4xx.h>

#define LED0_PIN     GPIO_Pin_9
#define LED1_PIN     GPIO_Pin_10

void led_init();

#endif
