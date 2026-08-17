#ifndef _SPI_H
#define _SPI_H


#include <stm32f4xx.h>
#include "sys.h"

#define SPI_CS   PBout(14)
#define SPI_MISO PBin(4)
#define SPI_MOSI PBout(5)
#define SPI_CLK  PBout(3)

uint8_t Spi_Mode0_SendRecvByte(uint8_t byte);
uint8_t Spi_Mode3_SendRecvByte(uint8_t byte);

#endif
