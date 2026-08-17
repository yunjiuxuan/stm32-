#include "iic.h"
#include "24c02.h"
#include "delay.h"
//起始信号
void iic_start()
{
	IIC_DATA_MODE_OUT;//设置输出模式
	IIC_SCL=1;
	IIC_SDA_OUT=1;
	delay_us(5);
	IIC_SDA_OUT=0;
	delay_us(5);
	IIC_SCL=0;
	delay_us(5);
}


//停止信号
void iic_stop()
{
	IIC_DATA_MODE_OUT;//设置输出模式
	IIC_SCL=1;
	IIC_SDA_OUT=0;
	delay_us(5);
	IIC_SDA_OUT=1;
	delay_us(5);
	IIC_SDA_OUT=0;
}

//发送应答信号  ack 0应答   1非应答
void iic_sendAck(uint8_t ack)
{
	IIC_DATA_MODE_OUT;//设置输出模式
	IIC_SCL=0;
	IIC_SDA_OUT=ack;
	delay_us(2);
	IIC_SCL=1;
	delay_us(5);
	IIC_SCL=0;
	delay_us(5);
}


//接收应答信号  返回值: 0应答   1不应答
uint8_t iic_recvAck(void)
{
	uint8_t ack;
	IIC_DATA_MODE_IN;//设置输入模式
	IIC_SCL=1;
	delay_us(5);
	if(IIC_SDA_IN==1)
		ack=1;
	else
		ack=0;
	delay_us(5);
	
	IIC_SCL=0;
	delay_us(5);
	return ack;
}

//发送一个字节数据
void iic_sendbyte(uint8_t byte)
{
	int8_t i;
	IIC_DATA_MODE_OUT;//设置输出模式
	IIC_SCL=0;
	IIC_SDA_OUT=0;
	delay_us(5);
	
	//MSB
	for(i=7;i>=0;i--)
	{
		if(byte&(1<<i))
			IIC_SDA_OUT=1;
		else
			IIC_SDA_OUT=0;
		
		delay_us(2);//让其有时间发送出去
		
		IIC_SCL=1;
		delay_us(5);
		
		IIC_SCL=0;
		delay_us(5);
	}
}


//接收一个字节数据
uint8_t iic_recvbyte(void)
{
	int8_t i;
	uint8_t data=0;
	IIC_DATA_MODE_IN;//设置输入模式

	//MSB
	for(i=7;i>=0;i--)
	{
		IIC_SCL=1;
		delay_us(5);
		
		if(IIC_SDA_IN==1)
			data |= (1<<i);
		
		delay_us(2);//让其有时间接收
		
		IIC_SCL=0;
		delay_us(5);
	}
	
	return data;
}
