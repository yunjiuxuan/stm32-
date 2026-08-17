#include "sr04_task.h"
#include "oled_display.h"
#include "dsp_usart.h"
#include "delay.h"
void SR04_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(SR04_RCC, ENABLE);

	GPIO_InitStructure.GPIO_Pin 	= SR04_TRIG;
	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType 	= GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd	= GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Speed 	= GPIO_Speed_100MHz;
	GPIO_Init(SR04_PIN_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = SR04_ECHO;
	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_IN;
	GPIO_Init(SR04_PIN_PORT, &GPIO_InitStructure);

	GPIO_ResetBits(SR04_PIN_PORT, SR04_TRIG);
}

// 超声波测距，返回距离(cm)
int SR04_GET_Distance(void)
{
 
	
	
	uint32_t time = 0;
	GPIO_SetBits(SR04_PIN_PORT ,SR04_TRIG);
	delay_us(15);
	GPIO_ResetBits(SR04_PIN_PORT ,SR04_TRIG);
	while(GPIO_ReadInputDataBit(SR04_PIN_PORT, SR04_ECHO) != SET);
	
	while(GPIO_ReadInputDataBit(SR04_PIN_PORT, SR04_ECHO) == SET)
	{
		delay_us(1);
		time++;
	}
	return (time*0.034/2);
 
}

// 有人检测（每秒调一次）
uint8_t SR04_CheckPerson(int curr_dist )
{
	static int prev_dist = 0;
	static uint8_t person_state = 0;
	static uint8_t SR04_nopercount = 0;
	static uint8_t SR04_havepercount = 0;
 
	uint8_t instant_person = 0;

	if(curr_dist > 50)
	{
		instant_person = 0;
	}
	else
	{
		float diff = curr_dist - prev_dist;
		if(diff < 0) diff = -diff;

		if(diff > SR04_MOVE_THRESHOLD)
			instant_person = 1;
		else
			instant_person = 0;
	}

	if(instant_person == 1)
	{ 
		if(SR04_havepercount < SR04_HAVEPERSON_TIMEOUT)
			SR04_havepercount++;
			SR04_nopercount=0;
		if(SR04_havepercount >= SR04_HAVEPERSON_TIMEOUT    ){
			person_state = 1;
		
		}
	}
	else
	{
		 
		
		
		
		if(SR04_nopercount < SR04_NOPERSON_TIMEOUT)
				SR04_havepercount=0;
				SR04_nopercount++;
		if(SR04_nopercount >= SR04_NOPERSON_TIMEOUT)
			person_state = 0;
		 
	}

	prev_dist = curr_dist;
	return person_state;
}
