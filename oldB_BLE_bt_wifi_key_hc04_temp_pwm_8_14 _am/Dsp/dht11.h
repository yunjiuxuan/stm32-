#ifndef _DHT11_H
#define _DHT11_H

#include <stm32f4xx.h>

void dht11_init();
int8_t dht11_getval(uint8_t pbuf[5]);
#endif
