#include "stm32f4xx.h"

#include <string.h>
#include <stdio.h>

/* 自定义头文件 */
#include "func_led.h"
#include "dsp_usart.h"
#include "dsp_adc.h"
#include "delay.h"

uint8_t _logflg = 0;
uint16_t data;
uint8_t cmd[50];
uint8_t BLE_String[64];
uint8_t Humi_Temp[5];


int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	Delay_Init();
	PA9_10_USART1_Init(115200);
	PA5_ADC1_Init();
	
	while(1)
	{
		uint16_t val = Get_Adc_Value(ADC_Channel_5);
		printf("V:[%.4lf v]\r\n",val*1.0/4096*3.3);
		
		delay_s(1);
	}
}

