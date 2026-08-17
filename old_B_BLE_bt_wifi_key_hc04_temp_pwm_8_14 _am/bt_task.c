#include "main.h"
 
void BT_TurnOn(void)
{
	if(bt_state == STATUS_OFF && bt_handle != NULL)
	{  OLED_ShowCHinese(92, 3, IDX_HAN_KAI);

		xTaskNotifyGive(bt_handle);
		BLE_PRINTF("[BT] 发送开启通知\r\n");
	}
	else
	{
		  OLED_ShowCHinese(92, 3, IDX_HAN_KAI);

		BLE_PRINTF("[BT] 已开启，跳过\r\n");
	}
}

void BT_TurnOff(void)
{
	if(bt_state != STATUS_OFF)
	{
		 OLED_ShowCHinese(92, 3, IDX_HAN_GUAN);

		bt_disc_req = 1;  // 请求任务自己关掉
		BLE_PRINTF("[BT] 发送关闭请求\r\n");
	}
	else
	{
		 OLED_ShowCHinese(92, 3, IDX_HAN_GUAN);

		BLE_PRINTF("[BT] 已关闭，跳过\r\n");
	}
}

void BT_Toggle(void)
{
	if(bt_state == STATUS_OFF)
		BT_TurnOn();
	else
		BT_TurnOff();
}


void bt_task(void* pvParameters)
{
	uint8_t first_run = 1;  // 首次默认开蓝牙，方便APP看调试信息
	for(;;)
	{
		// 非首次：阻塞等开启信号
		if(!first_run)
		{
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
			BLE_PRINTF("[BT] 收到开启信号\r\n");
		}
		first_run = 0;
		BLE_PRINTF("[BT] 准备开启蓝牙\r\n");

		bt_state = STATUS_ON;
		OLED_SetBTStatus(STATUS_ON);
		USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
		BT_ClearOldCmd();
		BLE_PRINTF("[BT] 蓝牙已开启\r\n");

		uint16_t bt_timeout_cnt = 0;  // 超时计数，5秒没消息就降回ON
		uint16_t state_dbg_cnt = 0;
		while(1)
		{
			// 收到关闭请求就退出
			if(bt_disc_req)
			{
				bt_disc_req = 0;
				BLE_PRINTF("[BT] 收到关闭请求\r\n");
				break;
			}
 

			// 处理蓝牙APP新命令
			if(_bleflg == 1 && strlen((char *)BLE_String) != 0)
			{
				USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
				int ret = Process_Command(BLE_String[0]);
				if(ret == 0)
					BLE_PRINTF("[BT] 命令执行成功\r\n");
				else
					BLE_PRINTF("[BT] 命令执行失败\r\n");
				memset(BLE_String, 0, sizeof(BLE_String));
				_bleflg = 0;
				USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

				// 收到消息视为已连接，重置超时
				if(bt_state != STATUS_CONNECTED)
				{
					bt_state = STATUS_CONNECTED;
					OLED_SetBTStatus(STATUS_CONNECTED);
					BLE_PRINTF("[BT] 已连接 (收到消息)\r\n");
				}
				bt_timeout_cnt = 0;
			}

			// 重复命令只用来保活，不再执行
			if(_ble_keepalive == 1)
			{
				_ble_keepalive = 0;
				if(bt_state != STATUS_CONNECTED)
				{
					bt_state = STATUS_CONNECTED;
					OLED_SetBTStatus(STATUS_CONNECTED);
					BLE_PRINTF("[BT] 已连接 (重复命令保活)\r\n");
				}
				bt_timeout_cnt = 0;
			}

			// 5秒没消息就降回ON
			if(bt_state == STATUS_CONNECTED)
			{
				bt_timeout_cnt++;
				if(bt_timeout_cnt >= 100)
				{
					bt_timeout_cnt = 0;
					bt_state = STATUS_ON;
					OLED_SetBTStatus(STATUS_ON);
					BLE_PRINTF("[BT] 连接超时 (5秒无消息)\r\n");
				}
			}

		

			vTaskDelay(50);
		}

		USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
		memset(BLE_String, 0, sizeof(BLE_String));
		_bleflg = 0;
		_ble_keepalive = 0;
		bt_state = STATUS_OFF;
		OLED_SetBTStatus(STATUS_OFF);
		BLE_PRINTF("[BT] 蓝牙已关闭，等待开启信号...\r\n");
	}
}


