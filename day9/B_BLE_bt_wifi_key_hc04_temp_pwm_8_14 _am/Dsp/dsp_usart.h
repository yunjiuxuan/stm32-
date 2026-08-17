#ifndef __DSP_USART_H
#define __DSP_USART_H


#include "stm32f4xx.h"


#define CMD_GEAR_1   'a'
#define CMD_GEAR_2   'b'
#define CMD_GEAR_3   'c'
#define CMD_GEAR_OFF '0'
#define CMD_WIFI_DISC 'd'
#define CMD_BT_DISC   'e'
#define CMD_COUNTDOWN_START 'f'  // 启动倒计时(默认30分钟)/每次+30分钟
#define CMD_COUNTDOWN_STOP  'g'  // 关闭倒计时,显示00:00:00

extern uint8_t WIFI_String[256];
extern uint8_t wifi_flag;

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
void BT_State_Init(void);
uint8_t BT_Is_Connected(void);
void BT_ClearOldCmd(void);

void BT_TurnOn(void);
void BT_TurnOff(void);
void BT_Toggle(void);
void WiFi_TurnOn(void);
void WiFi_TurnOff(void);
void WiFi_Toggle(void);
extern int KeyNum;
/* ===== 倒计时功能全局变量(定义在 main.c 中) ===== */
extern volatile uint8_t  daojishi_state;      // 倒计时状态: 0=关闭, 1=开启
extern volatile uint32_t daojishi_remain_sec; // 倒计时剩余秒数

#endif
