#ifndef __MAIN_H
#define __MAIN_H

#include <string.h>
#include <stdio.h>

/* 自定义头文件 */
#include "func_led.h"
#include "dsp_usart.h"
#include "delay.h"
#include "iic.h"
#include "oled.h"
#include "oled_display.h"
#include "sys.h"
#include "led.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "key.h"

#include "dht11.h"
#include "key_task.h"
#include "sr04_task.h"

/* ===== 全局变量声明（定义在 main.c 中）===== */
extern uint8_t _logflg;
extern uint16_t data;
extern uint8_t cmd[50];
extern uint8_t BLE_String[64];
extern uint8_t _bleflg;
extern uint8_t _ble_keepalive;          // 重复命令标志：仅维持连接，不执行命令
extern volatile uint8_t wifi_disc_req;  // 断开巴法云请求（命令d）
extern volatile uint8_t bt_disc_req;    // 断开蓝牙请求（命令e）

extern TaskHandle_t bt_handle;          //蓝牙任务（供exti.c引用）
extern TaskHandle_t wifi_handle;        //WiFi任务（供exti.c引用）

extern volatile uint8_t wifi_state;
extern volatile uint8_t bt_state;


//互斥锁（在dsp_usart.c中定义并使用，main中创建）
extern SemaphoreHandle_t xUART_Mutex;
extern SemaphoreHandle_t xESP_Mutex;

void wifi_task(void* pvParameters);
void bt_task(void* pvParameters);

#endif /* __MAIN_H */
