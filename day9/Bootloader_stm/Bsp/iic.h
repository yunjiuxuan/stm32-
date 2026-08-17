#ifndef _IIC_H
#define _IIC_H

#include <stm32f4xx.h>
#include "oled.h"


#define AT24C02_IIC 0
#define OLED_IIC 1
#define IIC_SCL     PBout(15)
#define IIC_SDA_OUT PDout(10)
#define IIC_SDA_IN  PDin(10)

#if OLED_IIC
#define IIC_DATA_MODE_OUT oled_setmode(GPIO_Mode_OUT)
#define IIC_DATA_MODE_IN  oled_setmode(GPIO_Mode_IN)
#elif AT24C02_IIC
#define IIC_DATA_MODE_OUT at24c02_setmode(GPIO_Mode_OUT)
#define IIC_DATA_MODE_IN  at24c02_setmode(GPIO_Mode_IN)
#endif



void iic_start(void);

void iic_stop();

void iic_sendAck(uint8_t ack);


uint8_t iic_recvAck(void);

void iic_sendbyte(uint8_t byte);

uint8_t iic_recvbyte(void);


#endif
