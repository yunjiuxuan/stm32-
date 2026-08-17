#include "dsp_usart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "oled_display.h"
#include "pwm.h" 
extern uint8_t _bleflg;
/* ===== 倒计时功能全局变量 ===== */
volatile uint8_t  daojishi_state      = 0;  // 0=关闭, 1=开启
volatile uint32_t daojishi_remain_sec = 0;  // 剩余秒数

SemaphoreHandle_t xUART_Mutex = NULL;   // 串口2(BLE)发送互斥锁
SemaphoreHandle_t xESP_Mutex  = NULL;   // ESP8266收发互斥锁(递归)

int KeyNum = 0;   // 风扇档位: 0停 1/2/3档

#define BT_STATE_PORT      GPIOA
#define BT_STATE_PIN       GPIO_Pin_10
#define BT_STATE_CLK       RCC_AHB1Periph_GPIOA

#define BT_STATE_CLK_CMD(CLK, ENABLE)  RCC_AHB1PeriphClockCmd(CLK, ENABLE)
 
void BT_State_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    USART_Cmd(USART1, DISABLE);
}

uint8_t BT_Is_Connected(void)   // 读蓝牙STATE引脚: 1已连接 0未连接
{
    return (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10) == Bit_RESET) ? 1 : 0;
}

#define USART1_TX_GPIO_PIN		GPIO_Pin_9
#define USART1_RX_GPIO_PIN		GPIO_Pin_10
#define USART1_GPIO_PORT		GPIOA
void PA9_10_USART1_Init(uint32_t _baud)
{
	GPIO_InitTypeDef 	GPIO_InitStructure;
	USART_InitTypeDef 	USART_InitStructure;
	NVIC_InitTypeDef	NVIC_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9,  GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
	
	GPIO_InitStructure.GPIO_Pin 	= USART1_TX_GPIO_PIN | USART1_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd	= GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_OType	= GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed	= GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	USART_InitStructure.USART_BaudRate		= _baud;
	USART_InitStructure.USART_WordLength	= USART_WordLength_8b;
	USART_InitStructure.USART_Parity		= USART_Parity_No;
	USART_InitStructure.USART_Mode			= USART_Mode_Rx | USART_Mode_Tx;
	USART_InitStructure.USART_StopBits		= USART_StopBits_1;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART1, &USART_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority		 = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd	= ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	
	USART_Cmd(USART1, ENABLE);
}


extern uint8_t _logflg;
extern uint16_t data;
extern uint8_t cmd[50];
uint8_t cmd_tmp[50];

void USART1_IRQHandler(void)   // 串口1接收中断
{
	static uint8_t count = 0;
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
		
		data = USART_ReceiveData(USART1);
		if( data != '\n' )
		{
			if(count < sizeof(cmd_tmp) - 1)
			{
				cmd_tmp[count++] = data;
			}
			else
			{
				count = 0;
			}
		}
		else
		{
			cmd_tmp[count-1] = '\0';
			memset(cmd, 0, sizeof(cmd));
			strncpy((char *)cmd, (char *)cmd_tmp, count-1);
			memset(cmd_tmp, 0, sizeof(cmd_tmp));
			count = 0;
			_logflg = 1;
		}
	}
}


#define USART2_TX_GPIO_PIN		GPIO_Pin_2
#define USART2_RX_GPIO_PIN		GPIO_Pin_3
#define USART2_GPIO_PORT		GPIOA

void LOG_USART2_SEND(char *msg)
{
    uint8_t len = strlen(msg);
    for(uint8_t i = 0; i<len; i++)
    {
        USART_SendData(USART2, msg[i]);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    }
}

void BLE_PRINTF(const char *fmt, ...)   // 蓝牙格式化打印(用法同printf)
{
	char ble_buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(ble_buf, sizeof(ble_buf), fmt, args);
	va_end(args);

	if((xUART_Mutex != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
	{
		if(xSemaphoreTake(xUART_Mutex, portMAX_DELAY) == pdTRUE)
		{
			LOG_USART2_SEND(ble_buf);
			xSemaphoreGive(xUART_Mutex);
		}
	}
	else
	{
		LOG_USART2_SEND(ble_buf);
	}
}


void PA2_3_USART2_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

	GPIO_InitStructure.GPIO_Pin = USART2_TX_GPIO_PIN | USART2_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART2, &USART_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	USART_Cmd(USART2, ENABLE);
}	


uint8_t BLE_String_tmp[64];
extern uint8_t BLE_String[64];
extern uint8_t _bleflg;
extern uint8_t _ble_keepalive;
uint8_t ble_rx_count = 0;
static char oldcmd[64] = {0};   // 上次收到的命令,用于去重

void BT_ClearOldCmd(void)
{
	memset(oldcmd, 0, sizeof(oldcmd));
}

void USART2_IRQHandler(void)   // 蓝牙接收中断
{
	static uint8_t u2_count = 0;
	
	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
	{
		uint8_t rx_data = USART_ReceiveData(USART2);
		ble_rx_count++;
		
		if(rx_data == '\n')   // 收到换行,一帧结束
		{
			 
			if(u2_count > 0)
			{
				if(BLE_String_tmp[u2_count-1] == '\r')
					BLE_String_tmp[u2_count-1] = '\0';
				else
					BLE_String_tmp[u2_count] = '\0';
				
				if(strcmp((char*)BLE_String_tmp, oldcmd) == 0   &&  strcmp((char*)BLE_String_tmp, "f") != 0   )   // 和上次相同且不是f,仅维持连接
				{ 
					 
					BLE_PRINTF("[蓝牙RX] 重复/保活: %s   oldcmd：%s  curcmd:%s\r\n", BLE_String_tmp,oldcmd,BLE_String_tmp);
					_ble_keepalive = 1;
				}
				else   // 新命令,拷贝并通知主任务
				{
					strncpy(oldcmd, (char*)BLE_String_tmp, sizeof(oldcmd)-1);
					oldcmd[sizeof(oldcmd)-1] = '\0';
					
					strncpy((char *)BLE_String, (char *)BLE_String_tmp, sizeof(BLE_String)-1);
					BLE_String[sizeof(BLE_String)-1] = '\0';
					
					LOG_USART2_SEND("RX[");
					LOG_USART2_SEND((char *)BLE_String);
					LOG_USART2_SEND("]\r\n");
					BLE_PRINTF("[蓝牙RX] 新命令: %s\r\n", BLE_String);
					
					_bleflg = 1;
				}
			}
			
			memset(BLE_String_tmp, 0, sizeof(BLE_String_tmp));
			u2_count = 0;
		}
		else if(u2_count < sizeof(BLE_String_tmp)-1)
		{
			BLE_String_tmp[u2_count++] = rx_data;
		}
		else
		{
			LOG_USART2_SEND("ERR:缓冲区溢出!\r\n");
			memset(BLE_String_tmp, 0, sizeof(BLE_String_tmp));
			u2_count = 0;
		}
	}
}

#include "delay.h"


#define WIFI_USERNAME		"yjxhm"
#define WIFI_PASSWORD		"nshnshnsh"
#define UID					"3f2d5feab40ea3844d23321f165ab2db"
#define TOPIC_1				"urUhB6h6m004"


#define ESP8266_PIN_TX			GPIO_Pin_10
#define ESP8266_PIN_RX			GPIO_Pin_11
#define ESP8266_GPIOx_PORT		GPIOB
#define ESP8266_GPIOx_RCC		RCC_AHB1Periph_GPIOB
#define ESP8266_USART_RCC		RCC_APB1Periph_USART3
#define ESP8266_UART_RCCAPB1	1


void PB10_11_ESP8266_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_AHB1PeriphClockCmd(ESP8266_GPIOx_RCC, ENABLE);
	
	#if ESP8266_UART_RCCAPB1
		RCC_APB1PeriphClockCmd(ESP8266_USART_RCC, ENABLE);
	#else
		RCC_APB2PeriphClockCmd(ESP8266_USART_RCC, ENABLE);
	#endif
	
	GPIO_InitStructure.GPIO_Pin = ESP8266_PIN_TX | ESP8266_PIN_RX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(ESP8266_GPIOx_PORT, &GPIO_InitStructure);
	
	GPIO_PinAFConfig(ESP8266_GPIOx_PORT, GPIO_PinSource10, GPIO_AF_USART3);
	GPIO_PinAFConfig(ESP8266_GPIOx_PORT, GPIO_PinSource11, GPIO_AF_USART3);
	
	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART3, &USART_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	
	USART_Cmd(USART3, ENABLE);
}


uint8_t u3_count = 0;
#define RX_BUFF_SIZE			255
char recv_buff[RX_BUFF_SIZE] = {0};

uint8_t WIFI_String[256];
uint8_t wifi_flag = 0;
static char wifi_tmp[256];
static uint8_t wifi_tmp_cnt = 0;

void USART3_IRQHandler(void)   // ESP8266接收中断
{
	char data;
	if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
	{
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);
		
		data = USART_ReceiveData(USART3);
		if(u3_count < RX_BUFF_SIZE)
		{
			recv_buff[u3_count++] = data;
			recv_buff[u3_count] = '\0';
		}
		
		if(data == '\n')
		{
			if(wifi_tmp_cnt > 0)
			{
				if(wifi_tmp[wifi_tmp_cnt-1] == '\r')
					wifi_tmp[wifi_tmp_cnt-1] = '\0';
				else
					wifi_tmp[wifi_tmp_cnt] = '\0';
				
				if(strstr(wifi_tmp, "msg=") != NULL && strstr(wifi_tmp, "msg=OK") == NULL && strstr(wifi_tmp, "msg=FAIL") == NULL)
				{
					
					
			 	if(strcmp((char*)wifi_tmp, oldcmd) == 0    &&  strcmp((char*)wifi_tmp, "f") != 0   )   // 和上次相同或不是f,仅维持连接
				{
					
					BLE_PRINTF("[WiFi RX] 重复/保活: %s   oldcmd：%s  curcmd:%s\r\n", BLE_String_tmp,oldcmd,wifi_tmp);
			 
					_ble_keepalive = 1;
				}else{
					strncpy((char *)WIFI_String, wifi_tmp, sizeof(WIFI_String)-1);
					WIFI_String[sizeof(WIFI_String)-1] = '\0';
					BLE_PRINTF("[WiFi RX] 新命令: %s\r\n", WIFI_String);
					wifi_flag = 1;
				}
					
					
				}
			}
			memset(wifi_tmp, 0, sizeof(wifi_tmp));
			wifi_tmp_cnt = 0;
		}
		else if(wifi_tmp_cnt < sizeof(wifi_tmp)-1)
		{
			wifi_tmp[wifi_tmp_cnt++] = data;
		}
	}
}

int Parse_Bafa_Msg(char *raw, char *msg_out, uint16_t out_len)   // 解析巴法云msg字段
{
	char *p_start;
	uint16_t msg_len;
	
	p_start = strstr(raw, "msg=");
	if(p_start == NULL)
		return -1;
	
	p_start += 4;
	
	msg_len = strlen(p_start);
	
	if(msg_len > 0 && p_start[msg_len-1] == '\r')
		msg_len--;
	
	if(msg_len >= out_len)
		msg_len = out_len - 1;
	
	strncpy(msg_out, p_start, msg_len);
	msg_out[msg_len] = '\0';
	
	return 0;
}

int Process_Command(char cmd)   // 统一命令分发: a/b/c档 0停 d=WiFi e=蓝牙
{
	switch(cmd)
	{
		case CMD_GEAR_1:
			OLED_SetGear(1);
			KeyNum=1;
			PWM_SetCompare3(60);

			BLE_PRINTF("[cmd] a -> 1档\r\n");
			return 0;
		case CMD_GEAR_2:
			OLED_SetGear(2);
			KeyNum=2;
			PWM_SetCompare3(80);

			BLE_PRINTF("[cmd] b -> 2档\r\n");
			return 0;
		case CMD_GEAR_3:
			OLED_SetGear(3);
			KeyNum=3;
			PWM_SetCompare3(99);

			BLE_PRINTF("[cmd] c -> 3档\r\n");
			return 0;
		case CMD_GEAR_OFF:
			OLED_SetGear(0);
			PWM_SetCompare3(0);
			KeyNum=0;
			daojishi_state = 0;
			daojishi_remain_sec = 0; 
			BLE_PRINTF("[cmd] h -> 关闭倒计时并停止\r\n"); 
			return 0;
		case CMD_WIFI_DISC:
			WiFi_Toggle();
			BLE_PRINTF("[cmd] d -> WiFi切换\r\n");
			return 0;
		case CMD_BT_DISC:
			BT_Toggle();
			BLE_PRINTF("[cmd] e -> 蓝牙切换\r\n");
			return 0;
		case CMD_COUNTDOWN_START:
			if(daojishi_state == 0)
			{
				/* 倒计时未开启,启动倒计时,默认30分钟 */
				daojishi_remain_sec = 1 * 60;
				daojishi_state = 1;
					OLED_SetGear(2);
			KeyNum=2;
			PWM_SetCompare3(80);

				BLE_PRINTF("[cmd] f -> 启动倒计时 30分钟\r\n");
			}
			else
			{
				/* 倒计时已开启,增加30分钟 */
				daojishi_remain_sec += 30 * 60;
				
				BLE_PRINTF("[cmd] f -> 倒计时+30分钟, 剩余%lus\r\n", (unsigned long)daojishi_remain_sec);
			}
			return 0;
		case CMD_COUNTDOWN_STOP:
			daojishi_state = 0;
			daojishi_remain_sec = 0;
		 
			BLE_PRINTF("[cmd] g -> 关闭倒计时\r\n");
			
		
		
		
			return 0;
		






		default:
			BLE_PRINTF("[cmd] '%c' -> 未知命令\r\n", cmd);
			return -1;
	}
}

void ESP8266_Send_AT(const char *AT_MSG)
{
	uint8_t i;
	uint8_t locked = 0;
	if((xESP_Mutex != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
	{
		xSemaphoreTakeRecursive(xESP_Mutex, portMAX_DELAY);
		locked = 1;
	}
	for(i=0; i<strlen(AT_MSG); i++)
	{
		USART_SendData(USART3, AT_MSG[i]);
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	}
	if(locked) xSemaphoreGiveRecursive(xESP_Mutex);
}

int ESP8266_Recv_ACK(const char *AT_ACK, int timeout)
{
	int cnt=0,ret = -1;
	while(1)
	{
		if(strstr(recv_buff,AT_ACK) != NULL)
		{
			ret = 0;
			break;
		}
		else
		{
			delay_ms(10);
			cnt++;
			if(cnt>(timeout/10))
			{
				ret = -1;
				break;
			}
		}
	}
	BLE_PRINTF("[调试] 期望:'%s' | 收到:'%s' | 结果=%d\r\n", AT_ACK, recv_buff, ret);
	u3_count = 0;
	memset(recv_buff,0,RX_BUFF_SIZE);
	return ret;
}

int ESP8266_Connect_Server(void)   // 连接巴法云服务器
{
	uint8_t locked = 0;
	if((xESP_Mutex != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
	{
		xSemaphoreTakeRecursive(xESP_Mutex, portMAX_DELAY);
		locked = 1;
	}

		BLE_PRINTF("[巴法云] 正在连接...\r\n");
	int result = 0;

	vTaskDelay(1000);
	ESP8266_Send_AT("+++");
	vTaskDelay(1000);
 int ack_ret;
for(int i=0;i<3;i++){
	ESP8266_Send_AT("AT\r\n");
	ack_ret = ESP8266_Recv_ACK("OK", 3000);
	BLE_PRINTF("[步骤] AT测试 结果=%d\r\n", ack_ret);
	if(ack_ret == -1)
		BLE_PRINTF("[提示] 退出透传模式失败 (可能原本就不在透传)\r\n");
	else{ 
		BLE_PRINTF("[OK] 已退出透传模式\r\n"); 
		break;
	}
}
	

	ESP8266_Send_AT("AT+CWMODE=3\r\n");
	ack_ret = ESP8266_Recv_ACK("OK", 3000);
	BLE_PRINTF("[步骤] CWMODE=3 结果=%d\r\n", ack_ret);
	if(ack_ret == -1)
	{
		BLE_PRINTF("[失败] AT+CWMODE=3 超时\r\n");
		result = -1;
		goto connect_done;
	}

	char WIFI_PASS[64] = {0};
	sprintf(WIFI_PASS,"AT+CWJAP=\"%s\",\"%s\"\r\n",WIFI_USERNAME, WIFI_PASSWORD);
	ESP8266_Send_AT(WIFI_PASS);
	if(ESP8266_Recv_ACK("OK", 8000) == -1)
	{
		BLE_PRINTF("[失败] AT+CWJAP WiFi连接超时\r\n");
		result = -1;
		goto connect_done;
	}
	BLE_PRINTF("[OK] WiFi已连接\r\n");

	ESP8266_Send_AT("AT+CIPMODE=1\r\n");
	if(ESP8266_Recv_ACK("OK", 3000) == -1)
	{
		BLE_PRINTF("[失败] AT+CIPMODE=1 超时\r\n");
		result = -1;
		goto connect_done;
	}
	BLE_PRINTF("[OK] 透传模式已开启\r\n");

	ESP8266_Send_AT("AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n");
	if(ESP8266_Recv_ACK("OK", 10000) == -1)
	{
		BLE_PRINTF("[失败] AT+CIPSTART 巴法云连接超时\r\n");
		result = -1;
		goto connect_done;
	}
	BLE_PRINTF("[OK] 巴法云服务器已连接\r\n");

	ESP8266_Send_AT("AT+CIPSEND\r\n");
	if(ESP8266_Recv_ACK(">", 10000) == -1)
	{
		BLE_PRINTF("[失败] AT+CIPSEND 超时\r\n");
		result = -1;
		goto connect_done;
	}
	BLE_PRINTF("[OK] 透传发送模式已开启\r\n");

	char topic[128] = {0};
	sprintf(topic,"cmd=1&uid=%s&topic=%s\r\n",UID,TOPIC_1);
	ESP8266_Send_AT(topic);
	if(ESP8266_Recv_ACK("cmd=1&res=1", 3000) == -1)
	{
		BLE_PRINTF("[失败] 订阅主题超时\r\n");
		result = -1;
		goto connect_done;
	}
	BLE_PRINTF("[OK] 主题已订阅\r\n");

connect_done:
	if(locked) xSemaphoreGiveRecursive(xESP_Mutex);
	return result;
}
