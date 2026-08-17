#include "24c02.h"
#include "iic.h"


//PB8 SCL  
//PB9 SDA
void at24c02_init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	//B端口时钟使能(开启)
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//输出模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//高速
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

//设置SDA输入输出模式  PB9
int8_t at24c02_setmode(GPIOMode_TypeDef GPIO_Mode)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	if(GPIO_Mode==GPIO_Mode_OUT)
	{
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//输出模式
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//高速
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉		
	}
	else if(GPIO_Mode==GPIO_Mode_IN)
	{
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;//输入模式
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉	
	}
	else
		return -1;
	
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	return 0;
}


//写数据到24c02模块上
int8_t at24c02_pageWrite(uint8_t addr,uint8_t *pbuf,uint8_t len)
{
	uint8_t ack;
	iic_start();//开始信号
	iic_sendbyte(0xA0);//设备地址
	ack=iic_recvAck();//0有应答  1没应答
	if(ack==1)
		return -1;
	
	iic_sendbyte(addr);//24c02模块的写入地址
	ack=iic_recvAck();//0有应答  1没应答
	if(ack==1)
		return -2;
	
	while(len--)
	{
		iic_sendbyte(*pbuf);
		pbuf++;
		ack=iic_recvAck();//0有应答  1没应答
		if(ack==1)
			return -3;
	}
	
	iic_stop();//停止信号
	
	return 0;
}


//从24c02模块上读取数据
int8_t at24c02_read(uint8_t addr,uint8_t *pbuf,uint8_t len)
{
	uint8_t ack;
	iic_start();//开始信号
	iic_sendbyte(0xA0);//设备地址
	ack=iic_recvAck();//0有应答  1没应答
	if(ack==1)
		return -1;
	
	iic_sendbyte(addr);//24c02模块的写入地址
	ack=iic_recvAck();//0有应答  1没应答
	if(ack==1)
		return -2;
	
	iic_start();//开始信号
	iic_sendbyte(0xA1);//设备地址
	ack=iic_recvAck();//0有应答  1没应答
	if(ack==1)
		return -3;
	
	len--;
	while(len--)
	{
		*pbuf=iic_recvbyte();
		pbuf++;
		iic_sendAck(0);//发送应答信号
	}
	
	*pbuf=iic_recvbyte();
	iic_sendAck(1);//发送非应答信号
	
	
	iic_stop();//停止信号
	
	return 0;
}

