#include "dht11.h"
#include "sys.h"
#include "delay.h"
//dht11初始化
void dht11_init()
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	
	PGout(9)=1;//拉高，空闲状态
}

//切换输入输出模式
void set_mode(GPIOMode_TypeDef GPIO_Mode)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	if(GPIO_Mode==GPIO_Mode_OUT)
	{
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	}
	else if(GPIO_Mode==GPIO_Mode_IN)
	{
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	}
	else
		return;
		
	
	GPIO_Init(GPIOG, &GPIO_InitStructure);
}

//读取温湿度，40bit数据存到pbuf
int8_t dht11_getval(uint8_t pbuf[5])
{
	uint16_t count=0;
	int8_t i,j;
	uint8_t data=0;
	set_mode(GPIO_Mode_OUT);//输出模式
	
	PGout(9)=0;
	delay_ms(20);//拉低18ms以上，启动信号
	PGout(9)=1;
	delay_us(30);//释放总线20~40us
	
	set_mode(GPIO_Mode_IN);//切输入，准备接收
	
	
	while(PGin(9)==1)//等待从机拉低响应
	{
		count++;
		delay_us(1);
		if(count>=4000)
		{
			return -1;
		}	
	}
	
	count=0;
	while(PGin(9)==0)//从机响应的低电平约80us
	{
		count++;
		delay_us(1);
		if(count>=100)
		{
			return -2;
		}	
	}
	
	count=0;
	while(PGin(9)==1)//响应后的高电平约80us
	{
		count++;
		delay_us(1);
		if(count>=100)
		{
			return -3;
		}	
	}
	
	for(i=0;i<5;i++)
	{
		data=0;
		for(j=7;j>=0;j--)
		{
			count=0;
			while(PGin(9)==0)//每个bit前的50us低电平
			{
				count++;
				delay_us(1);
				if(count>=60)
				{
					return -4;
				}	
			}
			
			//用高电平时长区分0和1
			delay_us(40);//0的高电平约26~28us，1的高电平约70us
			
			if(PGin(9)==1)//还是高，说明是1
			{
				data |= 1<<j;
				
				count=0;
				while(PGin(9)==1)//等剩下的高电平结束
				{
					count++;
					delay_us(1);
					if(count>=50)
					{
						return -5;
					}	
				}
			}
		}
		pbuf[i]=data;//存一个字节
	}
	
	//校验和，前4字节之和等于第5字节
	if((pbuf[0]+pbuf[1]+pbuf[2]+pbuf[3])&0xff !=pbuf[4])
	{
		return -6;
	}
	
	delay_us(100);
	return 0;
}

