#ifndef _W25Q128_H
#define _W25Q128_H

#include <stm32f4xx.h>


void w25q128_init(void);
//��ȡ���̺��豸��ID
uint16_t w25q128_readID(void);
//��ȡָ����flash��ַ����
int8_t w25q128_readdata(uint32_t addr, uint8_t *pbuf, uint16_t len);
//��ȡ״̬�Ĵ���   0x05�Ĵ���1  0x35�Ĵ���2
uint8_t w25q128_readstatus(uint8_t num);
//дʹ�� 
void w25q128_writeEnable(void);
//дʧ�� 
void w25q128_writeDisable(void);
// Sector Erase 4KB
void w25q128_sectorErase(uint32_t addr);
// Block Erase 64KB (cmd 0xD8, typ 150ms)
void w25q128_blockErase64K(uint32_t addr);
// Block Erase 32KB (cmd 0x52)
void w25q128_blockErase32K(uint32_t addr);
// Page Program (max 256 bytes, no cross-page)
void w25q128_WritePageData(uint32_t addr,uint8_t *pbuf,uint16_t len);

//��������оƬ
void w25q128_ChipErase(void);

#endif
