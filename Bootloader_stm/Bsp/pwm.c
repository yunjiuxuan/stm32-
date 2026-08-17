#include "stm32f4xx.h"
#include "pwm.h"

 

// PWM初始化（TIM4通道3, PB8输出）
void pwm_timer4_init()
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_TIM4);
	
	
	TIM_TimeBaseInitStructure.TIM_Prescaler=840-1;
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period=100-1;
	TIM_TimeBaseInitStructure.TIM_ClockDivision=0;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);
	
	
	TIM_OCInitStructure.TIM_OCMode=TIM_OCMode_PWM1;                            
	TIM_OCInitStructure.TIM_OutputState=TIM_OutputState_Enable; 
	TIM_OCInitStructure.TIM_Pulse=0;     
	TIM_OCInitStructure.TIM_OCPolarity=TIM_OCPolarity_Low;   
	TIM_OC3Init(TIM4, &TIM_OCInitStructure);  
	
	TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);  
	
	TIM_ARRPreloadConfig(TIM4, ENABLE);
	
	TIM_Cmd(TIM4, ENABLE);
}

// 设置PWM比较值
void PWM_SetCompare3(uint8_t Compare)
{
	TIM_SetCompare3(TIM4, Compare);
}
