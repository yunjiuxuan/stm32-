//              GND   锟斤拷源锟斤拷
//              VCC   锟斤拷5V锟斤拷3.3v锟斤拷源
//              SCL   锟斤拷PB8锟斤拷SCL锟斤拷
//              SDA   锟斤拷PB9锟斤拷SDA锟斤拷       

#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"  	 
#include "delay.h"
#include "iic.h"
#include "dsp_usart.h"

void Write_IIC_Command(unsigned char IIC_Command)
{
   iic_start();
   iic_sendbyte(0x78);            //Slave address,SA0=0
   iic_recvAck();	
   iic_sendbyte(0x00);			//write command
   iic_recvAck();
   iic_sendbyte(IIC_Command); 
   iic_recvAck();	
   iic_stop();
}
/**********************************************
// IIC Write Data
**********************************************/
void Write_IIC_Data(unsigned char IIC_Data)
{
   iic_start();
   iic_sendbyte(0x78);            //Slave address,SA0=0
   iic_recvAck();	
   iic_sendbyte(0x40);			//write data
   iic_recvAck();
   iic_sendbyte(IIC_Data);
   iic_recvAck();	
   iic_stop();
}
void OLED_WR_Byte(unsigned dat,unsigned cmd)
{
	if(cmd)
			{

   Write_IIC_Data(dat);
   
		}
	else {
   Write_IIC_Command(dat);
		
	}


}


/********************************************
// fill_Picture
********************************************/
void fill_picture(unsigned char fill_Data)
{
	unsigned char m,n;
	for(m=0;m<8;m++)
	{
		OLED_WR_Byte(0xb0+m,0);		//page0-page1
		OLED_WR_Byte(0x00,0);		//low column start address
		OLED_WR_Byte(0x10,0);		//high column start address
		for(n=0;n<128;n++)
			{
				OLED_WR_Byte(fill_Data,1);
			}
	}
}


//锟斤拷锟斤拷锟斤拷锟斤拷

	void OLED_Set_Pos(unsigned char x, unsigned char y) 
{ 	OLED_WR_Byte(0xb0+y,OLED_CMD);
	OLED_WR_Byte(((x&0xf0)>>4)|0x10,OLED_CMD);
	OLED_WR_Byte((x&0x0f),OLED_CMD); 
}   	  
//锟斤拷锟斤拷OLED锟斤拷示    
void OLED_Display_On(void)
{
	OLED_WR_Byte(0X8D,OLED_CMD);  //SET DCDC锟斤拷锟斤拷
	OLED_WR_Byte(0X14,OLED_CMD);  //DCDC ON
	OLED_WR_Byte(0XAF,OLED_CMD);  //DISPLAY ON
}
//锟截憋拷OLED锟斤拷示     
void OLED_Display_Off(void)
{
	OLED_WR_Byte(0X8D,OLED_CMD);  //SET DCDC锟斤拷锟斤拷
	OLED_WR_Byte(0X10,OLED_CMD);  //DCDC OFF
	OLED_WR_Byte(0XAE,OLED_CMD);  //DISPLAY OFF
}		   			 
//锟斤拷锟斤拷锟斤拷锟斤拷,锟斤拷锟斤拷锟斤拷,锟斤拷锟斤拷锟斤拷幕锟角猴拷色锟斤拷!锟斤拷没锟斤拷锟斤拷一锟斤拷!!!	  
void OLED_Clear(void)
{
	u8 i,n;
	for(i=0;i<8;i++)
	{
		OLED_WR_Byte (0xb0+i,OLED_CMD);
		OLED_WR_Byte (0x00,OLED_CMD);
		OLED_WR_Byte (0x10,OLED_CMD);
		for(n=0;n<128;n++)OLED_WR_Byte(0,OLED_DATA);
	}
}
void OLED_On(void)  
{  
	u8 i,n;		    
	for(i=0;i<8;i++)  
	{  
		OLED_WR_Byte (0xb0+i,OLED_CMD);    //锟斤拷锟斤拷页锟斤拷址锟斤拷0~7锟斤拷
		OLED_WR_Byte (0x00,OLED_CMD);      //锟斤拷锟斤拷锟斤拷示位锟矫★拷锟叫低碉拷址
		OLED_WR_Byte (0x10,OLED_CMD);      //锟斤拷锟斤拷锟斤拷示位锟矫★拷锟叫高碉拷址   
		for(n=0;n<128;n++)OLED_WR_Byte(1,OLED_DATA); 
	} //锟斤拷锟斤拷锟斤拷示
}
//锟斤拷指锟斤拷位锟斤拷锟斤拷示一锟斤拷锟街凤拷,锟斤拷锟斤拷锟斤拷锟斤拷锟街凤拷
//x:0~127
//y:0~7
//mode:0,锟斤拷锟斤拷锟斤拷示;1,锟斤拷锟斤拷锟斤拷示				 
//size:选锟斤拷锟斤拷锟斤拷 16/12 
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 Char_Size)
{      	
	unsigned char c=0,i=0;	
		c=chr-' ';//锟矫碉拷偏锟狡猴拷锟街�			
		if(x>Max_Column-1){x=0;y=y+2;}
		if(Char_Size ==16)
			{
			OLED_Set_Pos(x,y);	
			for(i=0;i<8;i++)
			OLED_WR_Byte(F8X16[c*16+i],OLED_DATA);
			OLED_Set_Pos(x,y+1);
			for(i=0;i<8;i++)
			OLED_WR_Byte(F8X16[c*16+i+8],OLED_DATA);
			}
			else {	
				OLED_Set_Pos(x,y);
				for(i=0;i<6;i++)
				OLED_WR_Byte(F6x8[c][i],OLED_DATA);
				
			}
}
//m^n锟斤拷锟斤拷
u32 oled_pow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;    
	return result;
}				  
//锟斤拷示2锟斤拷锟斤拷锟斤拷
//x,y :锟斤拷锟斤拷锟斤拷锟�	 
//len :锟斤拷锟街碉拷位锟斤拷
//size:锟斤拷锟斤拷锟叫�
//mode:模式	0,锟斤拷锟侥Ｊ�;1,锟斤拷锟斤拷模式
//num:锟斤拷值(0~4294967295);	 		  
void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size2)
{         	
	u8 t,temp;
	u8 enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/oled_pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				OLED_ShowChar(x+(size2/2)*t,y,' ',size2);
				continue;
			}else enshow=1; 
		 	 
		}
	 	OLED_ShowChar(x+(size2/2)*t,y,temp+'0',size2); 
	}
} 
//锟斤拷示一锟斤拷锟街凤拷锟脚达拷
void OLED_ShowString(u8 x,u8 y,u8 *chr,u8 Char_Size)
{
	unsigned char j=0;
	while (chr[j]!='\0')
	{		OLED_ShowChar(x,y,chr[j],Char_Size);
			x+=8;
		if(x>120){x=0;y+=2;}
			j++;
	}
}
//锟斤拷示锟斤拷锟斤拷
void OLED_ShowCHinese(u8 x,u8 y,u8 no)
{      			    
	u8 t,adder=0;
	OLED_Set_Pos(x,y);	
    for(t=0;t<16;t++)
		{
				OLED_WR_Byte(Hzk[2*no][t],OLED_DATA);
				adder+=1;
     }	
		OLED_Set_Pos(x,y+1);	
    for(t=0;t<16;t++)
			{	
				OLED_WR_Byte(Hzk[2*no+1][t],OLED_DATA);
				adder+=1;
      }					
}
/***********锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷示锟斤拷示BMP图片128锟斤拷64锟斤拷始锟斤拷锟斤拷锟斤拷(x,y),x锟侥凤拷围0锟斤拷127锟斤拷y为页锟侥凤拷围0锟斤拷7*****************/
void OLED_DrawBMP(unsigned char x0, unsigned char y0,unsigned char x1, unsigned char y1,unsigned char BMP[])
{ 	
 unsigned int j=0;
 unsigned char x,y;
  
  if(y1%8==0) y=y1/8;      
  else y=y1/8+1;
	for(y=y0;y<y1;y++)
	{
		OLED_Set_Pos(x0,y);
    for(x=x0;x<x1;x++)
	    {      
	    	OLED_WR_Byte(BMP[j++],OLED_DATA);	    	
	    }
	}
} 

//initialize SSD1306
void OLED_Init(void)
{ 	 
 	GPIO_InitTypeDef  GPIO_InitStructure;
 	
 	//Enable GPIO clock for GPIOB (SCL=PB15) and GPIOD (SDA=PD10)
 	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOD, ENABLE);

	//Initialize SCL pin - PB15
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);

	//Initialize SDA pin - PD10
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	
	delay_ms(200);

	OLED_WR_Byte(0xAE,OLED_CMD);//--display off
	OLED_WR_Byte(0x00,OLED_CMD);//---set low column address
	OLED_WR_Byte(0x10,OLED_CMD);//---set high column address
	OLED_WR_Byte(0x40,OLED_CMD);//--set start line address  
	OLED_WR_Byte(0xB0,OLED_CMD);//--set page address
	OLED_WR_Byte(0x81,OLED_CMD); // contract control
	OLED_WR_Byte(0xFF,OLED_CMD);//--128   
	OLED_WR_Byte(0xA1,OLED_CMD);//set segment remap 
	OLED_WR_Byte(0xA6,OLED_CMD);//--normal / reverse
	OLED_WR_Byte(0xA8,OLED_CMD);//--set multiplex ratio(1 to 64)
	OLED_WR_Byte(0x3F,OLED_CMD);//--1/32 duty
	OLED_WR_Byte(0xC8,OLED_CMD);//Com scan direction
	OLED_WR_Byte(0xD3,OLED_CMD);//-set display offset
	OLED_WR_Byte(0x00,OLED_CMD);//
	
	OLED_WR_Byte(0xD5,OLED_CMD);//set osc division
	OLED_WR_Byte(0x80,OLED_CMD);//
	
	OLED_WR_Byte(0xD8,OLED_CMD);//set area color mode off
	OLED_WR_Byte(0x05,OLED_CMD);//
	
	OLED_WR_Byte(0xD9,OLED_CMD);//Set Pre-Charge Period
	OLED_WR_Byte(0xF1,OLED_CMD);//
	
	OLED_WR_Byte(0xDA,OLED_CMD);//set com pin configuartion
	OLED_WR_Byte(0x12,OLED_CMD);//
	
	OLED_WR_Byte(0xDB,OLED_CMD);//set Vcomh
	OLED_WR_Byte(0x30,OLED_CMD);//
	
	OLED_WR_Byte(0x8D,OLED_CMD);//set charge pump enable
	OLED_WR_Byte(0x14,OLED_CMD);//
	
	OLED_WR_Byte(0xAF,OLED_CMD);//--turn on oled panel
}  


int8_t oled_setmode(GPIOMode_TypeDef GPIO_Mode)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	//Only switch SDA mode (PD10), SCL (PB15) always output
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
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
	
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	
	return 0;
}


























