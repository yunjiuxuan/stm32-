#ifndef __DSP_USART_H
#define __DSP_USART_H


#include "stm32f4xx.h"
#include "w25q128.h"


#define CMD_LED_ON    'a'
#define CMD_LED_OFF   'b'

extern uint8_t WIFI_String[256];
extern uint8_t wifi_flag;

/* Bootloader USART1 ��ӡ����,������ main.c */
void Boot_Print(const char *s, ...);
/* OTA ������־:ͬʱ����� USART1(PC ����)�� USART2(����) */
void OTA_LOG(const char *fmt, ...);
 
int Parse_Bafa_Msg(char *raw, char *msg_out, uint16_t out_len);
void PA9_10_USART1_Init(uint32_t _baud);
void PA2_3_USART2_Init(uint32_t baudrate);
void LOG_USART2_SEND(char *msg);
void BLE_PRINTF(const char *fmt, ...);
void PB10_11_ESP8266_Init(uint32_t baudrate);
void ESP8266_Send_AT(const char *AT_MSG);
int ESP8266_Recv_ACK(const char *AT_ACK, int timeout);
int ESP8266_Connect_Server(void);
int OTA_HTTP_Test(void);

/* OTA 固件下载: 从 meta.url 下载 bin 文件到 W25Q128
 * url  : 固件下载地址 (如 http://bin.bemfa.com/xxx/xxx.bin)
 * 返回: 0=成功 -1=失败
 * 固件数据写入 W25Q128 的 0x00000000 起始处
 */
int codeDownload(const char *url);

/* OTA 搬运与跳转: 从 W25Q128 读取固件写入内部 Flash, 校验后设置跳转标志并复位
 * 流程:
 *   1. 判断当前分区(BKP1R), 决定写入到对侧分区 (A->B 或 B->A)
 *   2. 检测 W25Q128 数据是否带 HTTP 头, 计算固件偏移
 *   3. 解锁内部 Flash, 擦除目标分区的3个扇区 (128KB x 3 = 384KB)
 *   4. 从 W25Q128 读取固件, 按半字(16-bit)编程到内部 Flash
 *   5. memcmp 读回校验
 *   6. 设置 BKP0R 跳转标志, 触发系统复位
 * 返回: 0=成功 (实际不会返回,会复位) -1=失败
 */
int OTA_FlashAndJump(void);

/* 下载后可读取的全局状态 (extern 供外部使用) */
extern unsigned long g_bin_total_len;   /* Content-Length 指定的固件大小 */
extern unsigned long g_bin_recv_len;    /* 实际写入 W25Q128 的字节数     */

#endif


