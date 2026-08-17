#include "reg_led.h"


void LED_REG_Init(void)
{
	//使能外设时钟 GPIOF
	PUBLIC_RCC_AHB1ENR |= (0x01<<5);//将RCC_AHB1使能寄存器上的第6位置1，而保持其他位不变
	
	//选择GPIO的工作模式----输出模式
	GPIOx_MODER &= ~(0x01<<19);//置0
	GPIOx_MODER |= (0x01<<18);//置1

	//选择输出模式为推挽输出
	GPIOx_OTYPER &= ~(0x01<<9);
	
	//选择GPIO的输出速率---100MHz
	GPIOx_OSPEEDR |= (0x01<<19);
	GPIOx_OSPEEDR |= (0x01<<18);
	
	//设置上下拉---不拉/浮空
	GPIOx_PUPDR &= ~(0x01<<19);
	GPIOx_PUPDR &= ~(0x01<<18);
	
	GPIOx_ODR |= (0x01<<9);//PF9 输出高电平
}

