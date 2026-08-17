#include "func_led.h"

void Func_LED_Init(void)
{
	//PF9\PF10\PE13\PE14
	GPIO_InitTypeDef GPIO_InitStructure;
	/* ##### How to use this driver ##### */
	//1.如果需要使用GPIO，首先要打开AHB1的时钟线，\
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOx, ENABLE);
	/* 开启GPIOF的外设时钟线 */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	
	//2.使用GPIO外设要借助GPIO_Init()进行外设初始化，包含模式、输出类型、速度、上/下/不/拉
	GPIO_InitStructure.GPIO_Pin  	= GPIO_Pin_9 | GPIO_Pin_10;		//配置IO引脚
	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_OUT;	//配置输出模式
	GPIO_InitStructure.GPIO_OType 	= GPIO_OType_PP;	//配置推完输出
	GPIO_InitStructure.GPIO_PuPd	= GPIO_PuPd_NOPULL; //配置浮空输出
	GPIO_InitStructure.GPIO_Speed	= GPIO_Speed_100MHz;//配置高速输出
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin		= GPIO_Pin_13 | GPIO_Pin_14;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	
	//3.关闭LED灯---将PF9置1
	GPIO_SetBits(GPIOF, GPIO_Pin_9 | GPIO_Pin_10);//led 1 2
	GPIO_SetBits(GPIOE, GPIO_Pin_13 | GPIO_Pin_14);//led 3 4
	
}



