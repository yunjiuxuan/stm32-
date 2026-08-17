
#ifndef __DSP_SR04_H
#define __DSP_SR04_H


#include "stm32f4xx.h"

#define SR04_TRIG 		GPIO_Pin_7
#define SR04_ECHO 		GPIO_Pin_9
#define SR04_PIN_PORT	GPIOC
#define SR04_RCC		RCC_AHB1Periph_GPIOC

// 有人检测参数(cm)
#define SR04_DETECT_RANGE    35.0f
#define SR04_MOVE_THRESHOLD  1.0f

// 有人状态时效(每秒调一次)
#define SR04_NOPERSON_TIMEOUT  10
#define SR04_HAVEPERSON_TIMEOUT  2
 
void SR04_Init(void);
int SR04_GET_Distance(void);
uint8_t SR04_CheckPerson(int curr_dist);   // 返回1=有人,0=无人


#endif
