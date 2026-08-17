#ifndef _AT24C02_H
#define _AT24C02_H

#include <stm32f4xx.h>
#include "sys.h"

void at24c02_init();
int8_t at24c02_setmode(GPIOMode_TypeDef GPIO_Mode);
//从24c02模块上读取数据
int8_t at24c02_read(uint8_t addr,uint8_t *pbuf,uint8_t len);
//写数据到24c02模块上
int8_t at24c02_pageWrite(uint8_t addr,uint8_t *pbuf,uint8_t len);
#endif
