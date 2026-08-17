#include "dsp_usart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "stm32f4xx_flash.h"
#include "oled_display.h"

uint8_t _bleflg = 0;

/* =================================== USART1_LOG ===================================*/
#define USART1_TX_GPIO_PIN		GPIO_Pin_9
#define USART1_RX_GPIO_PIN		GPIO_Pin_10
#define USART1_GPIO_PORT		GPIOA
//USART1---PA9-PA10
void PA9_10_USART1_Init(uint32_t _baud)
{
	//???????y??????
	GPIO_InitTypeDef 	GPIO_InitStructure;
	USART_InitTypeDef 	USART_InitStructure;
	NVIC_InitTypeDef	NVIC_InitStructure;
	
	/* ##### How to use this driver ##### */
	//1.??????????
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	
	//2.????GPIO????????
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9,  GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
	
	//3.?????GPIO
	GPIO_InitStructure.GPIO_Pin 	= USART1_TX_GPIO_PIN | USART1_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd	= GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_OType	= GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed	= GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	//4.?????USART
	USART_InitStructure.USART_BaudRate		= _baud;//?????? 9600 115200
	USART_InitStructure.USART_WordLength	= USART_WordLength_8b;//???
	USART_InitStructure.USART_Parity		= USART_Parity_No;//???????????
	USART_InitStructure.USART_Mode			= USART_Mode_Rx | USART_Mode_Tx;//?????
	USART_InitStructure.USART_StopBits		= USART_StopBits_1;//????
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//?????????
	USART_Init(USART1, &USART_InitStructure);
	
	//5.?????NVIC
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority		 = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd	= ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	//6.????????????
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	
	//7.????????
	USART_Cmd(USART1, ENABLE);
}


uint8_t _logflg = 0;
uint16_t data = 0;
uint8_t cmd[50] = {0};
uint8_t cmd_tmp[50];

//?????????????
void USART1_IRQHandler(void)
{
	static uint8_t count = 0;
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
		
		data = USART_ReceiveData(USART1);
		if( data != '\n' )
			cmd_tmp[count++] = data;//abcde\r\n
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


/* =================================== USART2_BLE ===================================*/
#define USART2_TX_GPIO_PIN		GPIO_Pin_2
#define USART2_RX_GPIO_PIN		GPIO_Pin_3
#define USART2_GPIO_PORT		GPIOA

//???????????????2??????????
void LOG_USART2_SEND(char *msg)
{
    uint8_t len = strlen(msg);
    for(uint8_t i = 0; i<len; i++)
    {
        USART_SendData(USART2, msg[i]);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    }
}

//???????APP???????????????printf?????????USART2??
void BLE_PRINTF(const char *fmt, ...)
{
	char ble_buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(ble_buf, sizeof(ble_buf), fmt, args);
	va_end(args);
	LOG_USART2_SEND(ble_buf);
}


void PA2_3_USART2_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	//??????????
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	//????????????????GPIO????????????
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

	//?????GPIO
	GPIO_InitStructure.GPIO_Pin = USART2_TX_GPIO_PIN | USART2_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//???????
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//???????
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//?????????????01????????
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//?????????
	USART_InitStructure.USART_BaudRate = baudrate;//??????
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;//??????????????????????????
	USART_InitStructure.USART_Parity = USART_Parity_No;//???????????
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//1??????
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//??????????---???
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//???????????
	USART_Init(USART2, &USART_InitStructure);

	//???y????????NVIC
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;//???????????
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;//????????
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	//??????????
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	//??????
	USART_Cmd(USART2, ENABLE);
}	


uint8_t BLE_String_tmp[64];
uint8_t BLE_String[64] = {0};
uint8_t ble_rx_count = 0;

////?????????????
//void USART2_IRQHandler(void)
//{
//	static uint8_t u2_count = 0;
//	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
//	{
//				LOG_USART2_SEND("USART2_IRQHandler\r\n");
//	
//		//???????????
//		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
//		
//		//????????????????????
//		uint8_t rx_data = USART_ReceiveData(USART2);
//		 
//		while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
//		
//		if(rx_data == '\n')//?????????
//		{
//			if(u2_count > 0)
//			{
//				//?????????\r?????????
//				if(BLE_String_tmp[u2_count-1] == '\r')
//					u2_count--;
//				
//				memset(BLE_String, 0, sizeof(BLE_String));
//				strncpy((char *)BLE_String, (char *)BLE_String_tmp, u2_count);
//				_bleflg = 1;
//				
//				//????????????????????
//				BLE_PRINTF("[");
//				BLE_PRINTF((char *)BLE_String); 
//				BLE_PRINTF("]");
//				while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
//			}
//			memset(BLE_String_tmp, 0, sizeof(BLE_String_tmp));
//			u2_count = 0;
//		}
//		else if(u2_count < sizeof(BLE_String_tmp)-1)
//		{
//			BLE_String_tmp[u2_count++] = rx_data;
//		}
//	}
//}
void USART2_IRQHandler(void)
{
	static uint8_t u2_count = 0;
	
	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
	{
		uint8_t rx_data = USART_ReceiveData(USART2);
		ble_rx_count++;
		
		// ????????????????????
		if(rx_data == '\n')
		{
			if(u2_count > 0)
			{
				// ????????? \r
				if(BLE_String_tmp[u2_count-1] == '\r')
					BLE_String_tmp[u2_count-1] = '\0';
				else
					BLE_String_tmp[u2_count] = '\0';
				
				// ??????????????
				strncpy((char *)BLE_String, (char *)BLE_String_tmp, sizeof(BLE_String)-1);
				BLE_String[sizeof(BLE_String)-1] = '\0';
				
				// ?????APP?????????????
				LOG_USART2_SEND("RX[");
				LOG_USART2_SEND((char *)BLE_String);
				LOG_USART2_SEND("]\r\n");
				
				// ????????????main.c????
				_bleflg = 1;
			}
			
			// ????????????
			memset(BLE_String_tmp, 0, sizeof(BLE_String_tmp));
			u2_count = 0;
		}
		// ??????????????????
		else if(u2_count < sizeof(BLE_String_tmp)-1)
		{
			BLE_String_tmp[u2_count++] = rx_data;
		}
		else
		{
			// ???????????????????????
			LOG_USART2_SEND("ERR:Buffer overflow!\r\n");
			memset(BLE_String_tmp, 0, sizeof(BLE_String_tmp));
			u2_count = 0;
		}
	}
}

/* =================================== USART3_ESP8266 ===================================*/
#include "delay.h"


#define WIFI_USERNAME		"yjxhm"
#define WIFI_PASSWORD		"nshnshnsh"
#define UID					"3f2d5feab40ea3844d23321f165ab2db"
#define TOPIC_1				"ota"


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
	
	//??????????
	RCC_AHB1PeriphClockCmd(ESP8266_GPIOx_RCC, ENABLE);
	
	#if ESP8266_UART_RCCAPB1
		RCC_APB1PeriphClockCmd(ESP8266_USART_RCC, ENABLE);
	#else
		RCC_APB2PeriphClockCmd(ESP8266_USART_RCC, ENABLE);
	#endif
	
	//?????GPIO
	GPIO_InitStructure.GPIO_Pin = ESP8266_PIN_TX | ESP8266_PIN_RX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//???????
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//???????
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(ESP8266_GPIOx_PORT, &GPIO_InitStructure);
	
	//??????????
	GPIO_PinAFConfig(ESP8266_GPIOx_PORT, GPIO_PinSource10, GPIO_AF_USART3);
	GPIO_PinAFConfig(ESP8266_GPIOx_PORT, GPIO_PinSource11, GPIO_AF_USART3);
	
	//?????????
	USART_InitStructure.USART_BaudRate = baudrate;//??????
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;//??????????????????????????
	USART_InitStructure.USART_Parity = USART_Parity_No;//???????????
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//1??????
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//??????????---???
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//???????????
	USART_Init(USART3, &USART_InitStructure);
	
	//???y????????NVIC
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;//???????????
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;//????????
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	//??????????
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	
	//??????
	USART_Cmd(USART3, ENABLE);
}


uint8_t u3_count = 0;
#define RX_BUFF_SIZE			255
char recv_buff[RX_BUFF_SIZE] = {0};

uint8_t WIFI_String[256];
uint8_t wifi_flag = 0;
static char wifi_tmp[256];
static uint8_t wifi_tmp_cnt = 0;

//?????????????
void USART3_IRQHandler(void)
{
	char data;
	if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
	{
		//???????????
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);
		
		//????????????????????
		data = USART_ReceiveData(USART3);
		if(u3_count < RX_BUFF_SIZE)
		{
			recv_buff[u3_count++] = data;
			recv_buff[u3_count] = '\0';
		}
		
		//??\n???????????
		if(data == '\n')
		{
			if(wifi_tmp_cnt > 0)
			{
				//?????????\r
				if(wifi_tmp[wifi_tmp_cnt-1] == '\r')
					wifi_tmp[wifi_tmp_cnt-1] = '\0';
				else
					wifi_tmp[wifi_tmp_cnt] = '\0';
				
				//??????????????????ping?????????????OK/FAIL
				if(strstr(wifi_tmp, "msg=") != NULL && strstr(wifi_tmp, "msg=OK") == NULL && strstr(wifi_tmp, "msg=FAIL") == NULL)
				{
					strncpy((char *)WIFI_String, wifi_tmp, sizeof(WIFI_String)-1);
					wifi_flag = 1;
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

//????????????????msg???????
//???: cmd=2&uid=xxx&topic=xxx&msg=??????
//msg???????????????????????
//????: 0=??????msg, -1=??msg???
int Parse_Bafa_Msg(char *raw, char *msg_out, uint16_t out_len)
{
	char *p_start;
	uint16_t msg_len;
	
	//???? "msg=" ???
	p_start = strstr(raw, "msg=");
	if(p_start == NULL)
		return -1;
	
	p_start += 4;  //????"msg="
	
	//msg???????????????????????
	msg_len = strlen(p_start);
	
	//????????\r?????????
	if(msg_len > 0 && p_start[msg_len-1] == '\r')
		msg_len--;
	
	if(msg_len >= out_len)
		msg_len = out_len - 1;
	
	strncpy(msg_out, p_start, msg_len);
	msg_out[msg_len] = '\0';
	
	return 0;
}

//????????????
//????: 0=??????, -1=??????
int Process_Command(char cmd)
{
	switch(cmd)
	{
		case CMD_LED_ON:
			GPIO_ResetBits(GPIOF, GPIO_Pin_9);
			BLE_PRINTF("[命令] a -> LED点亮\r\n");
			return 0;
		case CMD_LED_OFF:
			GPIO_SetBits(GPIOF, GPIO_Pin_9);
			BLE_PRINTF("[命令] b -> LED熄灭\r\n");
			return 0;
		default:
			BLE_PRINTF("[命令] '%c' -> 未知命令\r\n", cmd);
			return -1;
	}
}

/**
  * @brief  ????AT??????,???????????????????       
  * @param  char *AT_MSG:?????AT??????????AT??????
  * @retval None
  */
void ESP8266_Send_AT(const char *AT_MSG)
{
	uint8_t i;
	for(i=0; i<strlen(AT_MSG); i++)
	{
		USART_SendData(USART3, AT_MSG[i]);
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	}
}

/**
  * @brief  ????AT??????,???????????????????
  * @param  
  *         char *AT_ACK:?????AT?????????????
  * @retval None
  */
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
	//?????????????????????????????????????
	BLE_PRINTF("[DBG] 期待:'%s' | 收到:'%s' | ret=%d\r\n", AT_ACK, recv_buff, ret);
	u3_count = 0;
	memset(recv_buff,0,RX_BUFF_SIZE);
	return ret;
}

int ESP8266_Connect_Server(void)
{
		BLE_PRINTF("[WiFi] 开始连接服务器\r\n");

	//??????????+++ ??????1???????????\r\n
	delay_s(1);                       //??????1??
	ESP8266_Send_AT("+++");           //????+++??????\r\n??
	delay_s(1);                       //?????1??ESP8266?????????

	//+++??????OK????AT???????????????
	ESP8266_Send_AT("AT\r\n");
	int ack_ret = ESP8266_Recv_ACK("OK", 3000);
	BLE_PRINTF("[调试] AT测试 ret=%d\r\n", ack_ret);
	if(ack_ret == -1)
	{
		BLE_PRINTF("[错误] AT无响应(检查ESP8266连接)\r\n");
	}
	else
	{
		BLE_PRINTF("[成功] AT通信正常\r\n");
	}

	//?????????????
	ESP8266_Send_AT("AT+CWMODE=3\r\n");//???STA+AP?????
	ack_ret = ESP8266_Recv_ACK("OK", 3000);
	BLE_PRINTF("[调试] CWMODE=3 ret=%d\r\n", ack_ret);
	if(ack_ret == -1)
	{
		BLE_PRINTF("[错误] AT+CWMODE=3 设置失败退出\r\n");
		return -1;
	}else{
		BLE_PRINTF("设置工作模式成功\r\n");
	}
	
	//???????????
	char WIFI_PASS[64] = {0};
	sprintf(WIFI_PASS,"AT+CWJAP=\"%s\",\"%s\"\r\n",WIFI_USERNAME, WIFI_PASSWORD);
	ESP8266_Send_AT(WIFI_PASS);//
	if(ESP8266_Recv_ACK("OK", 8000) == -1)
	{
		BLE_PRINTF("[错误] AT+CWJAP 连接WiFi失败\r\n");
		return -1;
	}else{
		BLE_PRINTF("连接WiFi   成功\r\n");
		BLE_PRINTF("WIFI_USERNAME    ");
		BLE_PRINTF(WIFI_USERNAME);
		BLE_PRINTF("WIFI_PASSWORD    ");
		BLE_PRINTF(WIFI_PASSWORD);
	}
	
	//???????
	ESP8266_Send_AT("AT+CIPMODE=1\r\n");
	if(ESP8266_Recv_ACK("OK", 3000) == -1)
	{
		BLE_PRINTF("[错误] AT+CIPMODE=1 设置透传失败\r\n");
		return -1;
	}else{
		BLE_PRINTF("透传模式设置成功\r\n");
	}
	
	//????????
	ESP8266_Send_AT("AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n");
	if(ESP8266_Recv_ACK("OK", 10000) == -1)
	{
		BLE_PRINTF("[错误] AT+CIPSTART TCP连接服务器失败\r\n");
		return -1;
	}else{
		BLE_PRINTF("TCP服务器连接成功\r\n");
	}
	
	//???????
	ESP8266_Send_AT("AT+CIPSEND\r\n");
	if(ESP8266_Recv_ACK(">", 10000) == -1)
	{
		BLE_PRINTF("[错误] AT+CIPSEND 进入透传失败\r\n");
		return -1;
	}else{
		BLE_PRINTF("进入透传模式成功\r\n");
	}
	
	//????????
	char topic[128] = {0};
	sprintf(topic,"cmd=1&uid=%s&topic=%s\r\n",UID,TOPIC_1);
	ESP8266_Send_AT(topic);
	if(ESP8266_Recv_ACK("cmd=1&res=1", 3000) == -1)
	{
		BLE_PRINTF("[错误] 订阅主题失败\r\n");
		return -1;
	}else{
		BLE_PRINTF("订阅主题成功\r\n");
	}
	
	return 0;
}

/* =================================== OTA_HTTP_Test ===================================*/
/*
 * ????????????? bemfa ?? HTTP ?????? bin.bemfa.com:80
 *       ???? HTTP GET ?????? bin ????????? HTTP ???????
 *       ??Content-Length??crc64ecma??bin ????????
 *       ????????? bin ??????????? Flash ????????????
 *
 * ?????
 *   - ???????? bemfa.com:8344 ?? MQTT ?????ESP8266 ???? 1 ?? TCP ?????
 *   - ??? CIPMODE=0 ???????????? +IPD ???????????? AT ???
 *   - HTTP ??? = HTTP ???? + \r\n\r\n + bin ???????
 */

/* OTA ???????:??????? USART1(PC ????)?? USART2(????) */
/* ???????????;??????(?? HTTP ????)????? Boot_Print ??? */
void OTA_LOG(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    BLE_PRINTF("%s", buf);
}

/* OTA HTTP ???????????????????????? */
#define OTA_BIN_FILENAME  "ota.bin"   /* ??????????? bin ?????   */
#define OTA_HTTP_HOST     "bin.bemfa.com"
#define OTA_HTTP_PORT     80
#define OTA_HTTP_PATH     "/b/340437/3BcM2YyZDVmZWFiNDBlYTM4NDRkMjMzMjFmMTY1YWIyZGI=ota.bin"

/* ?????????? HTTP ???????, ????? buff, ????????????? */
static int _HTTP_WaitForString(char *keyword, char *buff, uint16_t buff_size, int timeout_ms)
{
    int loop_cnt = 0;
    int total = 0;
    buff[0] = '\0';
    while (1)
    {
        /* ???? +IPD,len:data ???? data ???????? +IPD ??? */
        if (recv_buff[0] != '\0')
        {
            char *pdata = strstr(recv_buff, "+IPD,");
            if (pdata != NULL)
            {
                /* ??? ':' ????????????????? HTTP ???? */
                char *pcolon = strchr(pdata, ':');
                if (pcolon != NULL)
                {
                    pcolon++;
                    uint16_t dlen = strlen(pcolon);
                    if (total + dlen < buff_size - 1)
                    {
                        memcpy(buff + total, pcolon, dlen);
                        total += dlen;
                        buff[total] = '\0';
                    }
                }
            }
            /* ????????, ????????? */
            u3_count = 0;
            memset(recv_buff, 0, RX_BUFF_SIZE);
        }
        /* ?????????????????? */
        if (keyword != NULL && strstr(buff, keyword) != NULL)
        {
            return 0;
        }
        delay_ms(10);
        loop_cnt++;
        if (loop_cnt > (timeout_ms / 10))
        {
            return -1;
        }
        /* ???????????????????????????????????? */
        if (keyword == NULL && total > 0 && loop_cnt > (300 / 10))
        {
            return 0;
        }
    }
}

int OTA_HTTP_Test(void)
{
    static char http_buff[1024];    /* HTTP ???????? (1KB ????????) */
    int  ret;

    OTA_LOG("\r\n===== [OTA] HTTP bin 信息获取 =====\r\n");

    /* -------- 步骤 1:初始化 ESP8266 模块 -------- */
    /* 退出 MQTT 透传, 间隔 1s 发送 +++ */
    OTA_LOG("[OTA] 退出透传模式...\r\n");
    delay_s(1);
    ESP8266_Send_AT("+++");
    delay_s(1);

    ESP8266_Send_AT("AT\r\n");
    ret = ESP8266_Recv_ACK("OK", 3000);
    if (ret != 0)
    {
        OTA_LOG("[OTA] AT 无响应, 继续下一步...\r\n");
    }

    /* 关闭现有连接 (MQTT on 8344) */
    ESP8266_Send_AT("AT+CIPCLOSE\r\n");
    ESP8266_Recv_ACK("OK", 2000);

    /* 设置模式 + WiFi 连接 (复用原有的 AT) */
    OTA_LOG("[OTA] 连接 WiFi (STA模式)...\r\n");
    ESP8266_Send_AT("AT+CWMODE=3\r\n");
    if (ESP8266_Recv_ACK("OK", 3000) != 0)
    {
        OTA_LOG("[OTA-错误] CWMODE 设置失败\r\n");
        return -1;
    }

    char WIFI_PASS[64];
    sprintf(WIFI_PASS, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_USERNAME, WIFI_PASSWORD);
    ESP8266_Send_AT(WIFI_PASS);
    if (ESP8266_Recv_ACK("OK", 8000) != 0)
    {
        OTA_LOG("[OTA-错误] WiFi 连接失败 (检查账号密码?\r\n");
        return -1;
    }
    OTA_LOG("[OTA] WiFi 连接成功 (热点: %s)\r\n", WIFI_USERNAME);

    /* 设置单连接 + 非透传模式 */
    ESP8266_Send_AT("AT+CIPMUX=0\r\n");        /* 单连接模式 */
    ESP8266_Recv_ACK("OK", 2000);
    ESP8266_Send_AT("AT+CIPMODE=0\r\n");        /* 非透传 (关键!透传无法解析 HTTP) */
    if (ESP8266_Recv_ACK("OK", 2000) != 0)
    {
        OTA_LOG("[OTA-错误] CIPMODE=0 设置失败\r\n");
        return -1;
    }

    /* -------- 步骤 2:建立 TCP 连接到 bin.bemfa.com:80 -------- */
    char tcpcmd[64];
    sprintf(tcpcmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", OTA_HTTP_HOST, OTA_HTTP_PORT);
    OTA_LOG("[OTA] 连接到 %s:%d ...\r\n", OTA_HTTP_HOST, OTA_HTTP_PORT);
    ESP8266_Send_AT(tcpcmd);
    /* 成功: CONNECT OK 或 OK; 失败: ERROR 或 CLOSED */
    if (ESP8266_Recv_ACK("CONNECT", 10000) != 0)
    {
        OTA_LOG("[OTA-错误] TCP 连接失败 (检查bin.bemfa.com:80 可达)\r\n");
        return -1;
    }
    OTA_LOG("[OTA] TCP 连接成功\r\n");

    /* -------- 步骤 3:发送 HTTP GET 请求 -------- */
    /* HTTP GET 请求格式:
       GET /<uid>/<OTA_BIN_FILENAME> HTTP/1.1\r\n
       Host: bin.bemfa.com\r\n
       Connection: close\r\n
       \r\n
    */
    char http_req[160];
    int  http_req_len = sprintf(http_req,
                                "GET %s HTTP/1.1\r\n"
                                "Host: %s\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                OTA_HTTP_PATH, OTA_HTTP_HOST);

    /* 通知 ESP8266 即将发送数据长度 */
    char cipcmd[32];
    sprintf(cipcmd, "AT+CIPSEND=%d\r\n", http_req_len);
    ESP8266_Send_AT(cipcmd);
    if (ESP8266_Recv_ACK(">", 3000) != 0)
    {
        OTA_LOG("[OTA-错误] 未收到 CIPSEND 提示符 '>'\r\n");
        ESP8266_Send_AT("AT+CIPCLOSE\r\n");
        return -1;
    }
    /* 收到 ">", 发送实际 HTTP 请求 */
    ESP8266_Send_AT(http_req);
    OTA_LOG("[OTA] HTTP GET 已发送 (%d 字节):\r\n", http_req_len);
    BLE_PRINTF("%s\r\n", http_req);



    /* -------- 步骤 4:接收 HTTP 响应头 -------- */
    /* 核心: 只把 HTTP 响应头纯文本存进 http_buff, 一旦遇到 \r\n\r\n 立即停止,
     * 防止后续的二进制 bin 正文污染 http_buff (否则 strlen/strstr 会被 0x00 截断) */
    OTA_LOG("[OTA] 等待服务器响应...\r\n");
    {
        int loop_cnt = 0;
        int total = 0;                      /* http_buff 中已确认的响应头长度 */
        int wait_cnt = 0;                   /* 无新数据的空闲周期计数 */
        int header_found = 0;               /* 是否已找到 \r\n\r\n 结束标志 */
        
        while (1)
        {
            /* 处理 recv_buff 中的 +IPD 数据包 */
            if (recv_buff[0] != '\0')
            {
                char *pdata = strstr(recv_buff, "+IPD,");
                if (pdata != NULL)
                {
                    /* 1. 解析 +IPD,<LEN>: 中的实际数据长度 LEN */
                    char *pnum = pdata + 5;      /* 跳过 "+IPD," */
                    int pdata_len = 0;
                    while (*pnum >= '0' && *pnum <= '9')
                    {
                        pdata_len = pdata_len * 10 + (*pnum - '0');
                        pnum++;
                    }
                    /* 现在 *pnum 指向 ':' 后面的实际数据部分 */
                    
                    /* 2. 拷贝数据到 http_buff (仅限响应头部分) */
                    if (pdata_len > 0 && *pnum == ':' && (total + pdata_len) < (int)sizeof(http_buff) - 1)
                    {
                        int copy_len = pdata_len;
                        
                        if (!header_found)
                        {
                            /* 在拷贝前, 先检查本次接收的数据中是否包含 \r\n\r\n */
                            int i;
                            for (i = 0; i < copy_len - 3; i++)
                            {
                                if (pnum[i] == '\r' && pnum[i+1] == '\n' &&
                                    pnum[i+2] == '\r' && pnum[i+3] == '\n')
                                {
                                    /* 找到, 只拷贝到这里(包括 \r\n\r\n) */
                                    copy_len = i + 4;
                                    header_found = 1;
                                    break;
                                }
                            }
                            
                            memcpy(http_buff + total, pnum, copy_len);
                            total += copy_len;
                            http_buff[total] = '\0';
                        }
                        /* 如果已找到结束标志, 就不再追加数据, 但 recv_buff 仍需清空 */
                    }
                }
                u3_count = 0;
                memset(recv_buff, 0, RX_BUFF_SIZE);
                wait_cnt = 0;   /* 有新数据到达, 重置空闲计数 */
            }
            
            /* 响应头结束标志已找到, 退出循环 */
            if (header_found)
                break;
                
            delay_ms(10);
            loop_cnt++;
            
            /* 有数据在接收, 重置空闲计数; 若无新数据, 则累加超时 */
            if (recv_buff[0] != '\0') wait_cnt = 0;
            else if (total > 0) wait_cnt++;
            
            /* 硬超时 20s 或 500ms 无新数据到达认为收完 (仅在未找到结束标志时生效) */
            if (!header_found && (loop_cnt > (20000 / 10) || (total > 0 && wait_cnt > (500 / 10))))
            {
                OTA_LOG("[OTA-警告] 接收响应头超时或空闲过久, 可能是响应头不完整\r\n");
                break;
            }
        }
    }


    /* -------- 步骤 5:解析 HTTP 响应头 -------- */
    OTA_LOG("\r\n===== HTTP 响应头 (前512字节) =====\r\n");
    http_buff[511] = '\0';

    BLE_PRINTF("%s\r\n", http_buff);         /* USART2 蓝牙同步输出 */
    OTA_LOG("========================================\r\n\r\n");

    /* 解析 HTTP 状态码 (寻找 "HTTP/1.1 200 OK") */
    int http_status = 0;
    char *p = strstr(http_buff, "HTTP/");
    if (p != NULL)
    {
        sscanf(p, "HTTP/1.1 %d", &http_status);
        OTA_LOG("[OTA] HTTP 状态码: %d\r\n", http_status);
        if (http_status != 200)
        {
            OTA_LOG("[OTA-错误] 状态码不是 200 (404=文件不存在, 5xx=服务器错误)\r\n");
        }
    }

    /* 解析 Content-Length (bin 文件大小, 字节)  */
    uint32_t bin_len = 0;
    p = strstr(http_buff, "Content-Length:");
    if (p != NULL)
    {
        OTA_LOG("[OTA] 文件大小: %lu 字节 (%lu KB)\r\n",
                   (unsigned long)bin_len, (unsigned long)(bin_len / 1024));
    }
    else
        OTA_LOG("[OTA-错误] 响应头中未找到 Content-Length (是否分包发送?)\r\n");
    }

    /* 解析 Content-Type (验证是否为 bin 二进制文件) */
    p = strstr(http_buff, "Content-Type:");
    if (p != NULL)
    {
        char ctype[64];
        ctype[0] = '\0';
        /* 截取到 \r\n */
        char *pend = strstr(p, "\r\n");
        if (pend != NULL)
        {
            int len = (int)(pend - (p + 14));
            if (len > 63) len = 63;
            memcpy(ctype, p + 14, len);
            ctype[len] = '\0';
        }
        OTA_LOG("[OTA] 文件类型: %s\r\n", ctype);
    }

    /* 解析 crc64ecma (服务器自定义头部字段) */
    unsigned long long crc64 = 0;
    p = strstr(http_buff, "crc64ecma");
    if (p != NULL)
    {
        /* 找到 ':' 或 ': ' */
        char *pcol = strchr(p, ':');
        if (pcol != NULL)
        {
            sscanf(pcol + 1, "%llu", &crc64);
            OTA_LOG("[OTA] crc64ecma 校验: %llu\r\n", crc64);
        }
    }
    else
    {
        OTA_LOG("[OTA-错误] 响应头中未找到 crc64ecma (上传时未设置校验?)\r\n");
    }

    /* 解析 bin 数据起始偏移: HTTP 头结束之后的 \r\n\r\n */
    uint16_t bin_offset = 0;
    p = strstr(http_buff, "\r\n\r\n");
    if (p != NULL)
    {
        bin_offset = (uint16_t)((uint8_t *)(p + 4) - (uint8_t *)http_buff);
        OTA_LOG("[OTA] bin 数据起始偏移: %hu (包括 %hu 字节响应头)\r\n",
                   bin_offset, bin_offset);

        /* 调试屏幕显示: 打印 bin 前 4 字节 (查看是否为栈顶/入口地址格式) */
        if (bin_offset + 8 <= 1024 && strlen(http_buff) >= (unsigned)(bin_offset + 8))
        {
            OTA_LOG("[OTA] bin 前4字节 (十六进制): %02X %02X %02X %02X\r\n",
                       (uint8_t)http_buff[bin_offset],
                       (uint8_t)http_buff[bin_offset+1],
                       (uint8_t)http_buff[bin_offset+2],
                       (uint8_t)http_buff[bin_offset+3]);
        }
    }
    else
    {
        OTA_LOG("[OTA-错误] 响应数据中未找到响应头结束标志 CRLF*2 (响应头被截断, 需要重复接收数据)\r\n");
    }

    /* -------- 步骤 6:关闭连接, 测试结束 -------- */
    ESP8266_Send_AT("AT+CIPCLOSE\r\n");
    ESP8266_Recv_ACK("OK", 2000);
    OTA_LOG("[OTA] TCP 已关闭\r\n");

    if (http_status == 200 && bin_len > 0 && bin_offset > 0)
    {
        OTA_LOG("\r\n===== [OTA] HTTP 测试通过! =====\r\n");
        OTA_LOG("  文件大小 : %u 字节\r\n", (unsigned)bin_len);
        OTA_LOG("  crc64    : %llu\r\n", crc64);
        OTA_LOG("  头偏移   : %u 字节 (包括HTTP头)\r\n", (unsigned)bin_offset);
        OTA_LOG("  下载地址 : %s%s\r\n", OTA_HTTP_HOST, OTA_HTTP_PATH);
        OTA_LOG("==================================\r\n");
        return 0;
    }
    else
    {
        OTA_LOG("\r\n===== [OTA] HTTP 测试失败 (查看上述信息? =====\r\n");
        return -1;
    }
}







/* ===== OTA Download And Flash ===== */
int OTA_Download_And_Flash(uint32_t target_addr)
{
    static char http_buff[1024];
    int ret;
    int http_status = 0;
    uint32_t bin_len = 0;
    unsigned long long crc64 = 0;

    static uint8_t residual_buf[4096];
    int residual_len = 0;

    OTA_LOG("\r\n===== [OTA] 下载并写入 Flash =====\r\n");

    /* 步骤 1: WiFi + ESP8266 初始化 */
    OTA_LOG("[OTA] 退出透传模式...\r\n");
    delay_s(1);
    ESP8266_Send_AT("+++");
    delay_s(1);

    ESP8266_Send_AT("AT\r\n");
    ESP8266_Recv_ACK("OK", 3000);

    ESP8266_Send_AT("AT+CIPCLOSE\r\n");
    ESP8266_Recv_ACK("OK", 2000);

    OTA_LOG("[OTA] 配置 WiFi STA模式...\r\n");
    ESP8266_Send_AT("AT+CWMODE=3\r\n");
    if (ESP8266_Recv_ACK("OK", 3000) != 0) {
        OTA_LOG("[OTA-错误] CWMODE 设置失败\r\n");
        OLED_OTA_ShowFail();
        return -1;
    }

    {
        char WIFI_PASS[64];
        sprintf(WIFI_PASS, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_USERNAME, WIFI_PASSWORD);
        ESP8266_Send_AT(WIFI_PASS);
        if (ESP8266_Recv_ACK("OK", 8000) != 0) {
            OTA_LOG("[OTA-错误] WiFi 连接失败\r\n");
            OLED_OTA_ShowFail();
            return -1;
        }
    }
    OTA_LOG("[OTA] WiFi 连接成功 (%s)\r\n", WIFI_USERNAME);

    ESP8266_Send_AT("AT+CIPMUX=0\r\n");
    ESP8266_Recv_ACK("OK", 2000);
    ESP8266_Send_AT("AT+CIPMODE=0\r\n");
    if (ESP8266_Recv_ACK("OK", 2000) != 0) {
        OTA_LOG("[OTA-错误] CIPMODE=0 设置失败\r\n");
        OLED_OTA_ShowFail();
        return -1;
    }

    /* 步骤 2: TCP 连接 */
    {
        char tcpcmd[64];
        sprintf(tcpcmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", OTA_HTTP_HOST, OTA_HTTP_PORT);
        OTA_LOG("[OTA] 连接到 %s:%d ...\r\n", OTA_HTTP_HOST, OTA_HTTP_PORT);
        ESP8266_Send_AT(tcpcmd);
        if (ESP8266_Recv_ACK("CONNECT", 10000) != 0) {
            OTA_LOG("[OTA-错误] TCP 连接失败\r\n");
            OLED_OTA_ShowFail();
            return -1;
        }
    }
    OTA_LOG("[OTA] TCP 连接成功\r\n");

    /* 步骤 3: 擦除目标 Flash 扇区 (在 HTTP GET 之前, 避免数据丢失) */
    OTA_LOG("[OTA] 正在擦除目标扇区...\r\n");
    FLASH_Unlock();

    {
        uint16_t sectors[3];
        if (target_addr == 0x08040000) {
            sectors[0] = FLASH_Sector_6;
            sectors[1] = FLASH_Sector_7;
            sectors[2] = FLASH_Sector_8;
        } else {
            sectors[0] = FLASH_Sector_9;
            sectors[1] = FLASH_Sector_10;
            sectors[2] = FLASH_Sector_11;
        }

        int i;
        for (i = 0; i < 3; i++) {
            FLASH_Status status = FLASH_EraseSector(sectors[i], VoltageRange_3);
            if (status != FLASH_COMPLETE) {
                OTA_LOG("[OTA-错误] 擦除扇区 %d 失败: %d\r\n", sectors[i], status);
                FLASH_Lock();
                OLED_OTA_ShowFail();
                return -1;
            }
        }
    }
    OTA_LOG("[OTA] 擦除完成\r\n");

    /* 步骤 4: HTTP GET 请求 */
    {
        char http_req[160];
        int http_req_len = sprintf(http_req,
                                    "GET %s HTTP/1.1\r\n"
                                    "Host: %s\r\n"
                                    "Connection: close\r\n"
                                    "\r\n",
                                    OTA_HTTP_PATH, OTA_HTTP_HOST);

        char cipcmd[32];
        sprintf(cipcmd, "AT+CIPSEND=%d\r\n", http_req_len);
        ESP8266_Send_AT(cipcmd);
        if (ESP8266_Recv_ACK(">", 3000) != 0) {
            OTA_LOG("[OTA-错误] CIPSEND 未收到 '>'\r\n");
            ESP8266_Send_AT("AT+CIPCLOSE\r\n");
            FLASH_Lock();
            OLED_OTA_ShowFail();
            return -1;
        }
        ESP8266_Send_AT(http_req);
        OTA_LOG("[OTA] HTTP GET 已发送 (%d 字节)\r\n", http_req_len);
    }

    /* 步骤 5: 接收响应头 + 保存剩余 bin 数据 */
    OTA_LOG("[OTA] 等待服务器响应...\r\n");
    {
        int loop_cnt = 0;
        int total = 0;
        int header_found = 0;
        int wait_cnt = 0;

        while (1)
        {
            if (recv_buff[0] != '\0')
            {
                char *pdata = strstr(recv_buff, "+IPD,");
                if (pdata != NULL)
                {
                    char *pnum = pdata + 5;
                    int pdata_len = 0;
                    while (*pnum >= '0' && *pnum <= '9') {
                        pdata_len = pdata_len * 10 + (*pnum - '0');
                        pnum++;
                    }

                    if (pdata_len > 0 && *pnum == ':') {
                        pnum++;

                        if (!header_found) {
                            int i;
                            int copy_len = pdata_len;

                            for (i = 0; i < copy_len - 3; i++) {
                                if (pnum[i] == '\r' && pnum[i+1] == '\n' &&
                                    pnum[i+2] == '\r' && pnum[i+3] == '\n') {
                                    copy_len = i + 4;
                                    header_found = 1;
                                    break;
                                }
                            }

                            if (total + copy_len < (int)sizeof(http_buff) - 1) {
                                memcpy(http_buff + total, pnum, copy_len);
                                total += copy_len;
                                http_buff[total] = '\0';
                            }

                            if (header_found && copy_len < pdata_len) {
                                int remain = pdata_len - copy_len;
                                if (remain > (int)sizeof(residual_buf) - residual_len)
                                    remain = sizeof(residual_buf) - residual_len;
                                memcpy(residual_buf + residual_len, pnum + copy_len, remain);
                                residual_len += remain;
                            }
                        } else {
                            int remain = sizeof(residual_buf) - residual_len;
                            int copy = (pdata_len < remain) ? pdata_len : remain;
                            memcpy(residual_buf + residual_len, pnum, copy);
                            residual_len += copy;
                        }
                    }
                }
                u3_count = 0;
                memset(recv_buff, 0, RX_BUFF_SIZE);
                wait_cnt = 0;
            }

            if (header_found && residual_len >= (int)sizeof(residual_buf))
                break;

            if (header_found) {
                delay_ms(100);
                if (recv_buff[0] == '\0')
                    break;
            }

            delay_ms(10);
            loop_cnt++;

            if (recv_buff[0] != '\0') wait_cnt = 0;
            else if (total > 0) wait_cnt++;

            if (loop_cnt > (20000 / 10) || (total > 0 && wait_cnt > (500 / 10))) {
                OTA_LOG("[OTA-警告] 响应头接收超时\r\n");
                break;
            }
        }
    }

    /* 步骤 6: 解析响应头 */
    OTA_LOG("\r\n===== HTTP 响应头 =====\r\n");
    http_buff[511] = '\0';
    BLE_PRINTF("%s\r\n", http_buff);
    OTA_LOG("================================\r\n\r\n");

    {
        char *p = strstr(http_buff, "HTTP/");
        if (p != NULL) {
            sscanf(p, "HTTP/1.1 %d", &http_status);
            OTA_LOG("[OTA] HTTP 状态码: %d\r\n", http_status);
        }

        p = strstr(http_buff, "Content-Length:");
        if (p != NULL) {
            sscanf(p + 16, "%lu", (unsigned long *)&bin_len);
            OTA_LOG("[OTA] 文件大小: %lu 字节 (%lu KB)\r\n",
                       (unsigned long)bin_len, (unsigned long)(bin_len / 1024));
        }
    }

    /* 步骤 7: 验证响应 */
    if (http_status != 200 || bin_len == 0 || bin_len > 384 * 1024) {
        OTA_LOG("[OTA-错误] 无效响应 (状态码=%d 长度=%lu)\r\n", http_status, (unsigned long)bin_len);
        ESP8266_Send_AT("AT+CIPCLOSE\r\n");
        FLASH_Lock();
        OLED_OTA_ShowFail();
        return -1;
    }

    /* 步骤 8: 写入剩余数据到 Flash, 然后继续接收 */
    {
        uint32_t write_addr = target_addr;
        uint32_t total_received = 0;
        static uint8_t flash_buf[1024];
        int flash_idx = 0;
        int ridx = 0;

        OLED_OTA_ShowProgress(0);

        while (ridx < residual_len) {
            flash_buf[flash_idx++] = residual_buf[ridx++];
            total_received++;

            if (flash_idx >= 1024) {
                int w;
                for (w = 0; w < 1024; w += 4) {
                    uint32_t word = flash_buf[w] | (flash_buf[w+1] << 8) |
                                    (flash_buf[w+2] << 16) | (flash_buf[w+3] << 24);
                    FLASH_ProgramWord(write_addr + w, word);
                }
                write_addr += 1024;
                flash_idx = 0;
                {
                    uint8_t pct = (uint8_t)(total_received * 100 / bin_len);
                    OLED_OTA_ShowProgress(pct);
                }
                OTA_LOG("[OTA] 进度: %lu / %lu (%lu%%)\r\n",
                           (unsigned long)total_received,
                           (unsigned long)bin_len,
                           (unsigned long)(total_received * 100 / bin_len));
            }
        }

        /* 步骤 9: 通过 +IPD 接收剩余 bin 数据 */
        {
            int loop_cnt = 0;
            int wait_cnt = 0;

            while (total_received < bin_len)
            {
                if (recv_buff[0] != '\0')
                {
                    char *pdata = strstr(recv_buff, "+IPD,");
                    if (pdata != NULL)
                    {
                        char *pnum = pdata + 5;
                        int pdata_len = 0;
                        while (*pnum >= '0' && *pnum <= '9') {
                            pdata_len = pdata_len * 10 + (*pnum - '0');
                            pnum++;
                        }

                        if (pdata_len > 0 && *pnum == ':') {
                            pnum++;
                            {
                                int j;
                                for (j = 0; j < pdata_len && total_received < bin_len; j++) {
                                    flash_buf[flash_idx++] = pnum[j];
                                    total_received++;

                                    if (flash_idx >= 1024) {
                                        int w;
                                        for (w = 0; w < 1024; w += 4) {
                                            uint32_t word = flash_buf[w] | (flash_buf[w+1] << 8) |
                                                            (flash_buf[w+2] << 16) | (flash_buf[w+3] << 24);
                                            FLASH_ProgramWord(write_addr + w, word);
                                        }
                                        write_addr += 1024;
                                        flash_idx = 0;
                                        {
                                            uint8_t pct = (uint8_t)(total_received * 100 / bin_len);
                                            OLED_OTA_ShowProgress(pct);
                                        }
                                        OTA_LOG("[OTA] 进度: %lu / %lu (%lu%%)\r\n",
                                                   (unsigned long)total_received,
                                                   (unsigned long)bin_len,
                                                   (unsigned long)(total_received * 100 / bin_len));
                                    }
                                }
                            }
                        }
                    }
                    u3_count = 0;
                    memset(recv_buff, 0, RX_BUFF_SIZE);
                    wait_cnt = 0;
                }

                delay_ms(5);
                loop_cnt++;

                if (recv_buff[0] != '\0') wait_cnt = 0;
                else wait_cnt++;

                if (loop_cnt > (60000 / 5) || (total_received > 0 && wait_cnt > (5000 / 5))) {
                    OTA_LOG("[OTA-错误] 接收超时 (%lu/%lu)\r\n",
                               (unsigned long)total_received, (unsigned long)bin_len);
                    break;
                }
            }
        }

        /* 步骤 10: 写入剩余数据 (填充到4字节对齐) */
        if (flash_idx > 0) {
            while (flash_idx % 4 != 0) {
                flash_buf[flash_idx++] = 0xFF;
            }
            {
                int w;
                for (w = 0; w < flash_idx; w += 4) {
                    uint32_t word = flash_buf[w] | (flash_buf[w+1] << 8) |
                                    (flash_buf[w+2] << 16) | (flash_buf[w+3] << 24);
                    FLASH_ProgramWord(write_addr + w, word);
                }
            }
            write_addr += flash_idx;
        }

        FLASH_Lock();
        OTA_LOG("[OTA] Flash 写入完成, 总计: %lu 字节\r\n", (unsigned long)total_received);
    }

    /* 步骤 11: 关闭 TCP 并验证 */
    ESP8266_Send_AT("AT+CIPCLOSE\r\n");
    ESP8266_Recv_ACK("OK", 2000);

    {
        volatile uint32_t *pAddr = (volatile uint32_t *)target_addr;
        uint32_t sp = pAddr[0];
        uint32_t reset_handler = pAddr[1];
        int sp_valid = 0;

        if ((sp >= 0x20000000) && (sp < 0x20020000)) sp_valid = 1;
        else if ((sp >= 0x10000000) && (sp < 0x10010000)) sp_valid = 1;

        if (sp_valid && (reset_handler >= 0x08040000) && (reset_handler < 0x08100000) && (reset_handler & 1)) {
            OTA_LOG("[OTA] 验证通过! SP=0x%08X PC=0x%08X\r\n", sp, reset_handler);
            OLED_OTA_ShowSuccess();
            return 0;
        } else {
            OTA_LOG("[OTA-错误] 验证失败! SP=0x%08X PC=0x%08X\r\n", sp, reset_handler);
            OLED_OTA_ShowFail();
            return -1;
        }
    }
}