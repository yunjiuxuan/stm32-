#ifndef __DSP_USART_H
#define __DSP_USART_H


#include "stm32f4xx.h"


#define CMD_LED_ON    'a'
#define CMD_LED_OFF   'b'

extern uint8_t WIFI_String[256];
extern uint8_t wifi_flag;

/* Bootloader USART1 打印函数,定义在 main.c */
void Boot_Print(const char *s);
/* OTA 调试日志:同时输出到 USART1(PC 串口)和 USART2(蓝牙) */
void OTA_LOG(const char *fmt, ...);

int Process_Command(char cmd);
int Parse_Bafa_Msg(char *raw, char *msg_out, uint16_t out_len);
void PA9_10_USART1_Init(uint32_t _baud);
void PA2_3_USART2_Init(uint32_t baudrate);
void LOG_USART2_SEND(char *msg);
void BLE_PRINTF(const char *fmt, ...);
void PB10_11_ESP8266_Init(uint32_t baudrate);
void ESP8266_Send_AT(const char *AT_MSG);
int ESP8266_Recv_ACK(const char *AT_ACK, int timeout);
int ESP8266_Connect_Server(void);
int OTA_HTTP_Test(void);int OTA_Download_And_Flash(uint32_t target_addr);

#endif


