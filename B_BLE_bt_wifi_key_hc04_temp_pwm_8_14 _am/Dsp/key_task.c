#include "key_task.h"
#include "key.h"
#include "dsp_usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "led.h"
#include "func_beep.h"
#include "oled_display.h"
#include "oled.h"

extern void BT_Toggle(void);
extern void WiFi_Toggle(void);

/* ===== 分区定义 (Flash 三等分, A=B=384KB) ===== */
#define PARTITION_A_ADDR  0x08040000   /* A区起始地址 (扇区 6-8, 384KB) */
#define PARTITION_B_ADDR  0x080A0000   /* B区起始地址 (扇区 9-11, 384KB) */

/* ===== 分区切换请求标志 (写入 RTC 备份寄存器 BKP0R, 软复位后由 Bootloader 读取) ===== */
#define JUMP_FLAG_TO_A   0xA5A50001
#define JUMP_FLAG_TO_B   0xA5A50002
#define OTA_UPGRADE_A    0xA5A50003
#define OTA_UPGRADE_B    0xA5A50004
#define LONG_PRESS_3S     200          /* 3秒: 200次 × 15ms */

/* ===== 获取当前运行分区地址 ===== */
static uint32_t Get_Current_Partition(void)
{
    uint32_t addr = (uint32_t)Get_Current_Partition;
    if(addr >= PARTITION_B_ADDR)
        return PARTITION_B_ADDR;  /* B区 */
    else
        return PARTITION_A_ADDR;  /* A区 */
}

/* ===== 跳转到指定分区 ===== */
static void Jump_To_Partition(uint32_t addr)
{





	
    uint32_t app_sp;
    uint32_t app_pc;
    int sp_valid = 0;

    /* 检查栈顶值是否合法 (STM32F407ZE有两个SRAM区域) */
    /* SRAM1: 0x20000000 - 0x2001FFFF (128KB) */
    /* SRAM2: 0x10000000 - 0x10000FFFF (64KB) */
    app_sp = *(volatile uint32_t *)addr;
    
    if ((app_sp >= 0x20000000) && (app_sp < 0x20020000)) {
        sp_valid = 1;  /* SRAM1 范围合法 */
    } else if ((app_sp >= 0x10000000) && (app_sp < 0x10010000)) {
        sp_valid = 1;  /* SRAM2 范围合法 */
    }
    
    if (!sp_valid) {
        BLE_PRINTF("[JUMP] 目标 0x%08X 栈顶非法:0x%08X, 不在SRAM范围\r\n", addr, app_sp);
        return;
    }

    app_pc = *(volatile uint32_t *)(addr + 4);
    
    /* 检查 Reset_Handler 地址是否在 Flash 范围内 */
    if ((app_pc < 0x08040000) || (app_pc >= 0x08100000)) {
        BLE_PRINTF("[JUMP] 目标 0x%08X PC非法:0x%08X, 不在Flash范围\r\n", addr, app_pc);
        return;
    }
    
    /* 检查 Thumb 位 (最低位为 1) */
    if (!(app_pc & 1)) {
        BLE_PRINTF("[JUMP] 目标 0x%08X PC非法:0x%08X, Thumb位未置1\r\n", addr, app_pc);
        return;
    }
    
    BLE_PRINTF("[JUMP] 跳转 0x%08X SP=0x%08X PC=0x%08X\r\n", addr, app_sp, app_pc);

    /* 关闭中断 */
    __disable_irq();

    /* 关闭 SysTick */
    SysTick->CTRL = 0;
    SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;
    
    /* 清除 NVIC 中断 */
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 设置中断向量表 */
    SCB->VTOR = addr;

    /* 设置主栈指针 */
    __set_MSP(app_sp);
    
    /* 使能中断 */
    __enable_irq();

    /* 跳转 */
    ((void (*)(void))app_pc)();
}



static KeyHandle_TypeDef Key0;

static KeyHandle_TypeDef Key1;
static KeyHandle_TypeDef Key2;
static KeyHandle_TypeDef Key3;

static KeyHandle_TypeDef Key4;

 
 
 
void KeyP4_ClickedCallback(){   // KEY4: 风扇开关切换（短按）
	KeyNum > 0 ? (KeyNum = 0 ,Process_Command('0')) :  (KeyNum = 2 ,Process_Command('b'));

}

void KeyP4_LongPressedCallback(void){   // KEY4: 长按3秒切换扇区
  BEEP_Init();
	

	uint32_t current = Get_Current_Partition();
    uint32_t target;

    if(current == PARTITION_A_ADDR){
			
				GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 亮
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 灭
		delay_ms(300);
	GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 亮
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 灭
		delay_ms(300);
			GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 亮
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 灭
		delay_ms(300);
				
			
			
			
        target = PARTITION_B_ADDR;
        BLE_PRINTF("\r\n[KEY4] 长按3秒! A区 -> B区\r\n");
    }else{
			
			
			// A 区有效，LED0 闪 1 次后跳转
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 亮
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 灭
		delay_ms(500);
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 亮
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 灭
		delay_ms(500);
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 亮
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 灭
		delay_ms(500);
			
        target = PARTITION_A_ADDR;
        BLE_PRINTF("\r\n[KEY4] 长按3秒! B区 -> A区\r\n");
    }

    BLE_PRINTF("[KEY4] 当前:0x%08X 目标:0x%08X\r\n", current, target);
 
//		  GPIO_SetBits(GPIOF, GPIO_Pin_8);
//		  delay_ms(500);
//		  GPIO_ResetBits(GPIOF, GPIO_Pin_8);
//		  delay_ms(500);


 

		/* 触发软复位切换分区: 写标志到 RTC 备份寄存器后系统复位,
		   Bootloader 读取标志并跳转, 硬件复位确保所有外设干净(避免
		   APP间直接跳转时 OLED软件IIC总线死锁/TIM/USART残留导致花屏卡死) */
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
		PWR_BackupAccessCmd(ENABLE);
		RTC->BKP0R = (target == PARTITION_A_ADDR) ? OTA_UPGRADE_A : OTA_UPGRADE_B;
		BLE_PRINTF("[KEY4] 触发软复位 -> 0x%08X\r\n", target);
		delay_ms(50);  /* 等待串口输出完成 */
		NVIC_SystemReset();  /* 系统复位, 不会返回 */
}

void Key3_ClickedCallback(){   // KEY3: 切换蓝牙
	Process_Command(CMD_BT_DISC);
	BT_Toggle();
	BLE_PRINTF("KEY3: 蓝牙开关已切换\r\n");
	
	
			GPIO_ResetBits(GPIOF, LED0_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOF, LED0_PIN);
	
	
}

void Key2_ClickedCallback(){   // KEY2: 切换WiFi
	Process_Command(CMD_WIFI_DISC);
	WiFi_Toggle();
	BLE_PRINTF("KEY2: WiFi开关已切换\r\n");
	
	
			GPIO_ResetBits(GPIOF, LED1_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOF, LED1_PIN);
	
}

void Key1_ClickedCallback(){   // KEY1: 降档
	KeyNum--;	
	if (KeyNum <=0 )  {
				KeyNum=0;
		Process_Command(0);
	
	}
	 else if  (KeyNum ==0 ){
		
		Process_Command('0');
		
	}
	 else if  (KeyNum == 1 ){
		
		Process_Command('a');
		
	}else{
				Process_Command('b');
	}
	BLE_PRINTF("KEY1:降档\r\n");
	
		GPIO_ResetBits(GPIOE, LED2_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOE, LED2_PIN);
	
}

void Key0_ClickedCallback(){   // KEY0: 升档
	KeyNum++;	

	if (KeyNum >=3 )  {
				KeyNum=3;
		Process_Command('c');
	
	}
	else if(KeyNum == 2 ){
		
		Process_Command('b');
		
	}else{
				Process_Command('a');
	}
	BLE_PRINTF("KEY0:升档\r\n");


	GPIO_ResetBits(GPIOE, LED3_PIN); 
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOE, LED3_PIN);
	
}

void Keytask_Init(KeyHandle_TypeDef *Handle){

    Handle->keyprevious = 1;
	  Handle->keycurrent = 1;
    Handle->press_cnt = 0;
    Handle->long_triggered = 0;




}


void Key_scan(KeyHandle_TypeDef *Handle){   // 按键扫描,松手触发短按,长按触发长按回调


		Handle->keycurrent = GPIO_ReadInputDataBit(Handle->GPIO_Port, Handle->GPIO_Pin) ;

		/* 按键按下中 */
		if(Handle->keycurrent == 0)
		{
				Handle->press_cnt++;

				/* 达到长按阈值且未触发过 */
				if(Handle->press_cnt >= LONG_PRESS_3S &&
					 Handle->long_triggered == 0 &&
					 Handle->LongPressedCallback != NULL)
				{
						Handle->long_triggered = 1;
						Handle->LongPressedCallback();
				}
		}

		/* 检测松手（上升沿） */
		if(Handle->keyprevious == 0 && Handle->keycurrent == 1)
		{
				/* 只有长按未触发时才触发短按 */
				if(Handle->long_triggered == 0 && Handle->ClickedCallback != NULL)
				{
						Handle->ClickedCallback();
				}
				/* 松手后清零 */
				Handle->press_cnt = 0;
				Handle->long_triggered = 0;
		}

		Handle->keyprevious = Handle->keycurrent ;

}

void key_task(void *pvParameters)
{
	
	Key0.keynum = 0;
	Key0.GPIO_Port = 	KEY0_GPIO_Port;
	Key0.GPIO_Pin = KEY0_PIN;
	Key0.ClickedCallback = Key0_ClickedCallback;
	
	Keytask_Init(&Key0);
	
	
	Key1.keynum = 1;
	Key1.GPIO_Port = 	KEY1_GPIO_Port;
	Key1.GPIO_Pin = KEY1_PIN;
	Key1.ClickedCallback = Key1_ClickedCallback;
	
	Keytask_Init(&Key1);
	
	
	Key2.keynum = 2;
	Key2.GPIO_Port = 	KEY2_GPIO_Port;
	Key2.GPIO_Pin = KEY2_PIN;
	Key2.ClickedCallback = Key2_ClickedCallback;
	
	Keytask_Init(&Key2);
	
	Key3.keynum = 3;
	Key3.GPIO_Port = 	KEY3_GPIO_Port;
	Key3.GPIO_Pin = KEY3_PIN;
	Key3.ClickedCallback = Key3_ClickedCallback;
		Keytask_Init(&Key3);
	
	
	Key4.keynum = 4;
	Key4.GPIO_Port = 	KEY4_GPIO_Port;
	Key4.GPIO_Pin = KEY4_PIN;
	Key4.ClickedCallback = KeyP4_ClickedCallback;
	Key4.LongPressedCallback = KeyP4_LongPressedCallback;  /* 长按3秒切换扇区 */
		Keytask_Init(&Key4);
	 
	
	for(;;){
			Key_scan(&Key0);
		  	Key_scan(&Key1);
			Key_scan(&Key2);
			Key_scan(&Key3);
			Key_scan(&Key4);
		
		
		vTaskDelay(pdMS_TO_TICKS(15));
	}
}
