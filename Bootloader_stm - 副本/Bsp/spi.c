#include "spi.h"
#include "delay.h"

//����ֵ�ǽ��յ��ֽ�   �β��Ƿ��͵��ֽ�
uint8_t Spi_Mode0_SendRecvByte(uint8_t byte)
{
	int8_t i=0;
	uint8_t data=0;
	
	for(i=7;i>=0;i--)
	{
		if(byte&(1<<i))
			SPI_MOSI=1;
		else
			SPI_MOSI=0;
		
		SPI_CLK=1;//����ʱ������Ϊ�ߵ�ƽ  �������MISO���ݶ�ȡ
		delay_us(1);//������ʱ�����
		
		if(SPI_MISO==1)
		{
			data |= (1<<i);
		}
		
		SPI_CLK=0;//����ʱ������Ϊ�͵�ƽ  �������MOSI����д��
		delay_us(1);//������ʱ�����
	}
	
	return data;
}


//����ֵ�ǽ��յ��ֽ�   �β��Ƿ��͵��ֽ�
uint8_t Spi_Mode3_SendRecvByte(uint8_t byte)
{
	int8_t i=0;
	uint8_t data=0;
	
	for(i=7;i>=0;i--)
	{
		if(byte&(1<<i))
			SPI_MOSI=1;
		else
			SPI_MOSI=0;
		
		SPI_CLK=0;//����ʱ������Ϊ�͵�ƽ  �������MISO���ݶ�ȡ
		delay_us(2);//������ʱ�����
		
		if(SPI_MISO==1)
		{
			data |= 1<<i;
		}
		
		SPI_CLK=1;//����ʱ������Ϊ�ߵ�ƽ  �������MOSI����д��
		delay_us(2);//������ʱ�����
	}
	
	return data;
}




