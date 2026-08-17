#include "main.h"
#include "version.h"

/* ===== 全局变量定义（声明在 main.h 中，供其仿.c 文件 extern 引用＿==== */
uint8_t _logflg = 0;
uint16_t data;
uint8_t cmd[50];
uint8_t BLE_String[64];
uint8_t _bleflg = 0;
uint8_t _ble_keepalive = 0;
volatile uint8_t wifi_disc_req = 0;
volatile uint8_t bt_disc_req = 0;

TaskHandle_t bt_handle   = NULL;
TaskHandle_t wifi_handle = NULL;

volatile uint8_t wifi_state = STATUS_OFF;
volatile uint8_t bt_state   = STATUS_OFF;

/* Version info (stored in Flash, survives power-down) */
const version_info_t g_version = {
    VERSION_MAGIC, 1, 0, 20260814, "APP-A"
};


//运行时间计数（秒＿ 仿main.c 内部使用
static uint32_t g_run_sec = 0;
//app_task1 句柄 - 仿main.c 内部使用
static TaskHandle_t app_task1_handle = NULL;


/* ===== LED 模式指示辅助 ===== */
//LED0(PF9)=蓝牙指示, LED1(PF10)=WiFi指示
//三态：OFF=熄灭, ON=闪烁(每秒翻转), CONNECTED=常亮
static void update_mode_led(GPIO_TypeDef* GPIOx, uint16_t pin, uint8_t state, uint8_t* toggle_flag)
{
	switch(state)
	{
		case STATUS_CONNECTED:
			GPIO_ResetBits(GPIOx, pin);   //常亮(低有敿
			break;
		case STATUS_ON:
			*toggle_flag ^= 1;
			if(*toggle_flag) GPIO_ResetBits(GPIOx, pin);
			else             GPIO_SetBits(GPIOx, pin);
			break;
		default: //STATUS_OFF
		
			GPIO_SetBits(GPIOx, pin);     //熄灭
			break;
	}
}

/* ===== 系统任务：OLED刷新 + 计时 + USART1命令 + LED闪烁 ===== */
static void app_task1(void* pvParameters);
static void app_task1(void* pvParameters)
{
	static uint8_t bt_toggle = 0;
	static uint8_t wifi_toggle = 0;
	TickType_t last_wake = xTaskGetTickCount();  // 用于固定周期调度

	for(;;)
	{ 
		uint8_t dht_buf[5] = {0};//温度采集
		int curr_dist = SR04_GET_Distance();
		uint8_t has_person = SR04_CheckPerson(curr_dist);//检测有无人
		
		uint8_t temp = 24;//默认24庿
		if (dht11_getval(dht_buf) ==0  ){ 
		temp =	dht_buf[2]; 
		}		 

		/* ===== 倒计时递减(在OLED刷新之前执行,保证显示最新倿 ===== */
		if(daojishi_state && daojishi_remain_sec > 0)
		{
			daojishi_remain_sec--;
			if(daojishi_remain_sec == 0)
			{
				daojishi_state = 0;
				Process_Command(CMD_GEAR_OFF);  // 归零关风承
				BLE_PRINTF("[倒计时] 归零,风扇关闭\r\n");
			}
		}

		OLED_Display_Refresh(g_run_sec,curr_dist, has_person, temp);
 
		g_run_sec++;
 
		if(_logflg == 1 && strlen((char *)cmd) != 0)
		{
			int ret = Process_Command(cmd[0]);
			if(ret == 0)
				BLE_PRINTF("[USART1] 命令执行成功\r\n");
			else
				BLE_PRINTF("[USART1] 命令执行失败\r\n");
			memset(cmd, 0, sizeof(cmd));
			_logflg = 0;
		}
 
		vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
	}
}



int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	Delay_Init();
	PA9_10_USART1_Init(115200);
	BT_State_Init();
	PA2_3_USART2_Init(9600);
	PB10_11_ESP8266_Init(115200);
	Func_LED_Init();
	SR04_Init();
	dht11_init();
	pwm_timer4_init();
	//蓝牙/WiFi模块启动时处于OFF状态：关闭各自串口接收中断＿
	//避免在任务未开启前接收到数据堆积。任务开启时再使能?/关闭蓝牙/WiFi接收中断，防止初始化期间堆积数据
	USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
	USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
	
	//关闭USART1接收中断，防止初始化期间因噪声导致缓存溢出破坏内孿
	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);

	OLED_Init();
	OLED_Clear();
	OLED_Display_Init();
	
	//【重要】PA10 已被 BT_State_Init 用作蓝牙 STATE 输入＿
	//        不再恢复 USART1 皿RXNE 接收中断，否则会导致 USART1 空转
	//USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);  // 已禁甿

	//创建互斥锁（必须在调度器启动前创建）
	xUART_Mutex = xSemaphoreCreateMutex();
	xESP_Mutex  = xSemaphoreCreateRecursiveMutex();

	//启动时蓝牿WiFi均为OFF状怿
	OLED_SetBTStatus(STATUS_OFF);
	OLED_SetWiFiStatus(STATUS_OFF);
	OLED_Display_Refresh(g_run_sec, 0, 0, 0);  //启动时未采集传感器，伿

	BLE_PRINTF("\r\n=== %s V%d.%d build %d ===\r\n", g_version.name, g_version.ver_major, g_version.ver_minor, g_version.build_date);


 


	/* ===== 创建任务 ===== */
	//app_task1 优先线（系统任务，需稳定刷新OLED＿
	xTaskCreate((TaskFunction_t)app_task1,
			  (const char*    )"app_task1",
			  (uint16_t       )512,
			  (void*          )NULL,
			  (UBaseType_t    )3,
			  (TaskHandle_t*  )&app_task1_handle);

	//bt_task 优先线
	xTaskCreate((TaskFunction_t)bt_task,
			  (const char*    )"bt_task",
			  (uint16_t       )512,
			  (void*          )NULL,
			  (UBaseType_t    )2,
			  (TaskHandle_t*  )&bt_handle);
 
	xTaskCreate((TaskFunction_t)wifi_task,
			  (const char*    )"wifi_task",
			  (uint16_t       )768,
			  (void*          )NULL,
			  (UBaseType_t    )2,
			  (TaskHandle_t*  )&wifi_handle);
	xTaskCreate((TaskFunction_t)key_task,
			  (const char*    )"key_task",
			  (uint16_t       )128,
			  (void*          )NULL,
			  (UBaseType_t    )3,
			  (TaskHandle_t*  )NULL);
				
//	xTaskCreate((TaskFunction_t)key3_task,
//			  (const char*    )"key3_task",
//			  (uint16_t       )128,
//			  (void*          )NULL,
//			  (UBaseType_t    )1,
//			  (TaskHandle_t*  )NULL);
//	xTaskCreate((TaskFunction_t)key2_task,
//			  (const char*    )"key2_task",
//			  (uint16_t       )128,
//			  (void*          )NULL,
//			  (UBaseType_t    )1,
//			  (TaskHandle_t*  )NULL);
//	xTaskCreate((TaskFunction_t)key1_task,
//			  (const char*    )"key1_task",
//			  (uint16_t       )128,
//			  (void*          )NULL,
//			  (UBaseType_t    )1,
//			  (TaskHandle_t*  )NULL);
//	xTaskCreate((TaskFunction_t)key0_task,
//			  (const char*    )"key0_task",
//			  (uint16_t       )128,
//			  (void*          )NULL,
//			  (UBaseType_t    )1,
//			  (TaskHandle_t*  )NULL);
				
				
 
				
	//初始化按键与外部中断：必须在任务创建之后，确俿bt_handle/wifi_handle 非空＿
	//否则中断在句柄为NULL时触发会导致 xTaskNotifyFromISR 解引用NULL而HardFault
 
	vTaskStartScheduler();

	while(1)
	{
	}
}
