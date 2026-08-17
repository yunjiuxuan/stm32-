#include "24c02.h"
#include "iic.h"


//PB8 SCL
//PB9 SDA
void at24c02_init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

//切换SDA方向  PB9
int8_t at24c02_setmode(GPIOMode_TypeDef GPIO_Mode)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	if(GPIO_Mode==GPIO_Mode_OUT)
	{
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;		
	}
	else if(GPIO_Mode==GPIO_Mode_IN)
	{
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;	
	}
	else
		return -1;
	
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	return 0;
}


//写数据到24c02
int8_t at24c02_pageWrite(uint8_t addr,uint8_t *pbuf,uint8_t len)
{
	uint8_t ack;
	iic_start();//启动信号
	iic_sendbyte(0xA0);//设备地址
	ack=iic_recvAck();//等待应答
	if(ack==1)
		return -1;
	
	iic_sendbyte(addr);//写入地址
	ack=iic_recvAck();
	if(ack==1)
		return -2;
	
	while(len--)
	{
		iic_sendbyte(*pbuf);
		pbuf++;
		ack=iic_recvAck();
		if(ack==1)
			return -3;
	}
	
	iic_stop();//停止信号
	
	return 0;
}


//从24c02读取数据
int8_t at24c02_read(uint8_t addr,uint8_t *pbuf,uint8_t len)
{
	uint8_t ack;
	iic_start();//启动信号
	iic_sendbyte(0xA0);//设备地址
	ack=iic_recvAck();
	if(ack==1)
		return -1;
	
	iic_sendbyte(addr);//写入地址
	ack=iic_recvAck();
	if(ack==1)
		return -2;
	
	iic_start();//重启信号
	iic_sendbyte(0xA1);//读操作地址
	ack=iic_recvAck();
	if(ack==1)
		return -3;
	
	len--;
	while(len--)
	{
		*pbuf=iic_recvbyte();
		pbuf++;
		iic_sendAck(0);//主机应答
	}
	
	*pbuf=iic_recvbyte();
	iic_sendAck(1);//不发应答，准备结束
	
	
	iic_stop();//停止信号
	
	return 0;
}
