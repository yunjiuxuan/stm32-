#include "w25q128.h"
#include "spi.h"
#include "delay.h"

//ģ��spi
//F_CS  PB14
//MISO  PB4
//MOSI  PB5
//SCL   PB3
void w25q128_init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	//B�˿�ʱ��ʹ��(����)
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_5|GPIO_Pin_14;//PB3 PB5 PB14����
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//���ģʽ
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//�������
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//����
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;//��������
	GPIO_Init(GPIOB, &GPIO_InitStructure);//����GPIO����
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;//PB4����
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;//���ģʽ
	GPIO_Init(GPIOB, &GPIO_InitStructure);//����GPIO����
	
	SPI_CS=1;//ȡ��Ƭѡ
}



//��ȡ���̺��豸��ID
uint16_t w25q128_readID(void)
{
	uint8_t man_id=0;
	uint8_t dev_id=0;
	uint16_t id=0;
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(0x90);//������
	Spi_Mode0_SendRecvByte(0x00);
	Spi_Mode0_SendRecvByte(0x00);
	Spi_Mode0_SendRecvByte(0x00);
	
	man_id=Spi_Mode0_SendRecvByte(0xFF);
	dev_id=Spi_Mode0_SendRecvByte(0xFF);
	
	id=(man_id<<8)|dev_id;
	
	SPI_CS=1;//ȡ��Ƭѡ
	return id;
}

//��ȡָ����flash��ַ����
int8_t w25q128_readdata(uint32_t addr, uint8_t *pbuf, uint16_t len)
{
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(0x03);//������
	Spi_Mode0_SendRecvByte((addr>>16)&0xff);
	Spi_Mode0_SendRecvByte((addr>>8)&0xff);
	Spi_Mode0_SendRecvByte((addr>>0)&0xff);
	
	while(len--)
	{
		*pbuf=Spi_Mode0_SendRecvByte(0xFF);
		pbuf++;
		delay_us(2);
	}
	
	SPI_CS=1;//ȡ��Ƭѡ
	return 0;
}


//��ȡ״̬�Ĵ���   0x05�Ĵ���1  0x35�Ĵ���2
uint8_t w25q128_readstatus(uint8_t num)
{
	uint8_t statu=0;
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(num);//������
	statu=Spi_Mode0_SendRecvByte(0xFF);

	SPI_CS=1;//ȡ��Ƭѡ
	return statu;
}


//дʹ�� 
void w25q128_writeEnable(void)
{
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(0x06);//������
	
	SPI_CS=1;//ȡ��Ƭѡ
}

//дʧ�� 
void w25q128_writeDisable(void)
{
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(0x04);//������
	
	SPI_CS=1;//ȡ��Ƭѡ
}


//����ָ��������
void w25q128_sectorErase(uint32_t addr)
{
	uint8_t statu=0;
	w25q128_writeEnable();//дʹ��
	delay_us(10);
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(0x20);//������
	Spi_Mode0_SendRecvByte((addr>>16)&0xff);
	Spi_Mode0_SendRecvByte((addr>>8)&0xff);
	Spi_Mode0_SendRecvByte((addr>>0)&0xff);
	
	SPI_CS=1;//ȡ��Ƭѡ
	delay_us(10);
	while(1)
	{
		statu=w25q128_readstatus(0x05);//��ȡ״̬�Ĵ���1
		if((statu&0x01)==0)
			break;
		else
			delay_us(1);
	}
	
	w25q128_writeDisable();//дʧ��
	delay_us(10);
}


//��ָ����flash�ڴ��ַд������
// Block Erase 64KB (cmd 0xD8, typ 150ms, 5x faster than 16x sector erase)
void w25q128_blockErase64K(uint32_t addr)
{
	uint8_t statu=0;
	w25q128_writeEnable();
	delay_us(10);
	SPI_CS=0;

	Spi_Mode0_SendRecvByte(0xD8);
	Spi_Mode0_SendRecvByte((addr>>16)&0xff);
	Spi_Mode0_SendRecvByte((addr>>8)&0xff);
	Spi_Mode0_SendRecvByte((addr>>0)&0xff);

	SPI_CS=1;
	delay_us(10);
	while(1)
	{
		statu=w25q128_readstatus(0x05);
		if((statu&0x01)==0)
			break;
		else
			delay_us(1);
	}

	w25q128_writeDisable();
	delay_us(10);
}

// Block Erase 32KB (cmd 0x52)
void w25q128_blockErase32K(uint32_t addr)
{
	uint8_t statu=0;
	w25q128_writeEnable();
	delay_us(10);
	SPI_CS=0;

	Spi_Mode0_SendRecvByte(0x52);
	Spi_Mode0_SendRecvByte((addr>>16)&0xff);
	Spi_Mode0_SendRecvByte((addr>>8)&0xff);
	Spi_Mode0_SendRecvByte((addr>>0)&0xff);

	SPI_CS=1;
	delay_us(10);
	while(1)
	{
		statu=w25q128_readstatus(0x05);
		if((statu&0x01)==0)
			break;
		else
			delay_us(1);
	}

	w25q128_writeDisable();
	delay_us(10);
}

void w25q128_WritePageData(uint32_t addr,uint8_t *pbuf,uint16_t len)
{
	uint8_t statu=0;
	w25q128_writeEnable();//дʹ��
	delay_us(10);
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(0x02);//������
	Spi_Mode0_SendRecvByte((addr>>16)&0xff);
	Spi_Mode0_SendRecvByte((addr>>8)&0xff);
	Spi_Mode0_SendRecvByte((addr>>0)&0xff);
	
	while(len--)
	{
		Spi_Mode0_SendRecvByte(*pbuf);
		pbuf++;
		delay_us(1);
	}
	
	SPI_CS=1;//ȡ��Ƭѡ
	delay_us(10);
	
	
	while(1)
	{
		statu=w25q128_readstatus(0x05);//��ȡ״̬�Ĵ���1
		if((statu&0x01)==0)
			break;
		else
			delay_us(1);
	}
	
	w25q128_writeDisable();//дʧ��
	delay_us(10);
}



//��������оƬ
void w25q128_ChipErase(void)
{
	uint8_t statu=0;
	w25q128_writeEnable();//дʹ��
	delay_us(10);
	SPI_CS=0;//Ƭѡ
	
	Spi_Mode0_SendRecvByte(0xC7);//������
	
	SPI_CS=1;//ȡ��Ƭѡ
	delay_us(10);
	while(1)
	{
		statu=w25q128_readstatus(0x05);//��ȡ״̬�Ĵ���1
		if((statu&0x01)==0)//busy�Ƿ�æµ
			break;
		else
			delay_us(1);
	}
	
	w25q128_writeDisable();//дʧ��
	delay_us(10);
}
