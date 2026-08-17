#include "led.h"

//led初始化
void led_init()
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	//F端口时钟使能(开启)
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

	GPIO_InitStructure.GPIO_Pin = LED0_PIN | LED1_PIN;//PF9  PF10引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//输出模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//高速
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;//无上下拉
	GPIO_Init(GPIOF, &GPIO_InitStructure);//配置GPIO引脚 PF9 PF10
}






