#include "main.h"



void WiFi_TurnOn(void)
{
	if(wifi_state == STATUS_OFF && wifi_handle != NULL)
	{  OLED_ShowCHinese(92, 3, IDX_HAN_KAI);

		xTaskNotifyGive(wifi_handle);
		BLE_PRINTF("[WiFi] 发送开启通知\r\n");
	}
	else
	{  OLED_ShowCHinese(92, 3, IDX_HAN_KAI);

		BLE_PRINTF("[WiFi] 已开启，跳过\r\n");
	}
}

void WiFi_TurnOff(void)
{
	if(wifi_state != STATUS_OFF)
	{  OLED_ShowCHinese(92, 3, IDX_HAN_GUAN);

		wifi_disc_req = 1;  // 请求任务自己关掉
		BLE_PRINTF("[WiFi] 发送关闭请求\r\n");
	}
	else
	{OLED_ShowCHinese(92, 3, IDX_HAN_GUAN);

		BLE_PRINTF("[WiFi] 已关闭，跳过\r\n");
	}
}

void WiFi_Toggle(void)
{
	if(wifi_state == STATUS_OFF)
		WiFi_TurnOn();
	else
		WiFi_TurnOff();
}


void wifi_task(void* pvParameters)
{
	uint8_t first_run = 1;
	for(;;)
	{
		// 非首次：阻塞等开启信号
		if(!first_run)
		{
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
			BLE_PRINTF("[WiFi] 收到开启信号\r\n");
		}
		first_run = 0;

		wifi_state = STATUS_ON;
		OLED_SetWiFiStatus(STATUS_ON);
		USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
		BLE_PRINTF("[WiFi] WiFi已开启，连接巴法云...\r\n");

		uint8_t connect_ok = 0;
		uint8_t retry_cnt = 0;
		while(ESP8266_Connect_Server() != 0)
		{
			if(ulTaskNotifyTake(pdFALSE, 0) > 0 || wifi_disc_req)
				goto wifi_stop;   // 连不上又被要求关，直接走关闭流程
			BLE_PRINTF("[WiFi] 连接失败，重试次数: %d\r\n", ++retry_cnt);
			vTaskDelay(5000);
		}

		connect_ok = 1;
		wifi_state = STATUS_CONNECTED;
		OLED_SetWiFiStatus(STATUS_CONNECTED);
		BLE_PRINTF("[WiFi] 巴法云已连接\r\n");

		// 已连接：处理消息 + ping保活
		TickType_t last_ping = xTaskGetTickCount();
		for(;;)
		{
			// 收到关闭请求就退出
			if(wifi_disc_req)
			{
				wifi_disc_req = 0;
				BLE_PRINTF("[WiFi] 收到关闭请求\r\n");
				goto wifi_stop;
			}

			// 处理巴法云透传消息
			if(wifi_flag == 1 && strlen((char *)WIFI_String) != 0)
			{
				USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
				char bafa_msg[64] = {0};
				if(Parse_Bafa_Msg((char *)WIFI_String, bafa_msg, sizeof(bafa_msg)) == 0)
				{
					if(strlen(bafa_msg) > 0)
					{
						int ret = Process_Command(bafa_msg[0]);
						if(ret == 0)
							ESP8266_Send_AT("cmd=2&msg=OK\r\n");
						else
							ESP8266_Send_AT("cmd=2&msg=FAIL\r\n");
					}
				}
				else
				{
					BLE_PRINTF("[WiFi] 协议响应消息\r\n");
				}
				memset(WIFI_String, 0, sizeof(WIFI_String));
				wifi_flag = 0;
				USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
			}

			// 每秒ping一次保活
			if((xTaskGetTickCount() - last_ping) >= 1000)
			{
				ESP8266_Send_AT("ping\r\n");
				last_ping = xTaskGetTickCount();
			}

			vTaskDelay(50);
		}

	wifi_stop:
		BLE_PRINTF("[WiFi] 正在断开巴法云...\r\n");
		if(connect_ok)
		{
			ESP8266_Send_AT("+++");
			vTaskDelay(500);
			ESP8266_Send_AT("AT+CIPCLOSE\r\n");
		}
		USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
		memset(WIFI_String, 0, sizeof(WIFI_String));
		wifi_flag = 0;
		wifi_state = STATUS_OFF;
		OLED_SetWiFiStatus(STATUS_OFF);
		BLE_PRINTF("[WiFi] WiFi已关闭，等待开启信号...\r\n");
	}
}
