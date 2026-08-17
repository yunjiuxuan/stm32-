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

/* ===== �������� (Flash ���ȷ�, A=B=384KB) ===== */
#define PARTITION_A_ADDR  0x08040000   /* A����ʼ��ַ (���� 6-8, 384KB) */
#define PARTITION_B_ADDR  0x080A0000   /* B����ʼ��ַ (���� 9-11, 384KB) */

/* ===== �����л������־ (д�� RTC ���ݼĴ��� BKP0R, ����λ���� Bootloader ��ȡ) ===== */
#define JUMP_FLAG_TO_A   0xA5A50001
#define JUMP_FLAG_TO_B   0xA5A50002
#define OTA_UPGRADE_A    0xA5A50003
#define OTA_UPGRADE_B    0xA5A50004
#define LONG_PRESS_3S     200          /* 3��: 200�� �� 15ms */

/* ===== 获取当前运行分区地址 ===== */
/* noinline: 防止Keil -O2/-O3内联优化导致 (uint32_t)Get_Current_Partition 取地址失效 */
__attribute__((noinline)) static uint32_t Get_Current_Partition(void)
{
    uint32_t addr = (uint32_t)Get_Current_Partition;
    if(addr >= PARTITION_B_ADDR)
        return PARTITION_B_ADDR;  /* B区 */
    else
        return PARTITION_A_ADDR;  /* A区 */
}

/* ===== ��ת��ָ������ ===== */
static void Jump_To_Partition(uint32_t addr)
{





	
    uint32_t app_sp;
    uint32_t app_pc;
    int sp_valid = 0;

    /* ���ջ��ֵ�Ƿ�Ϸ� (STM32F407ZE������SRAM����) */
    /* SRAM1: 0x20000000 - 0x2001FFFF (128KB) */
    /* SRAM2: 0x10000000 - 0x10000FFFF (64KB) */
    app_sp = *(volatile uint32_t *)addr;
    
    if ((app_sp >= 0x20000000) && (app_sp < 0x20020000)) {
        sp_valid = 1;  /* SRAM1 ��Χ�Ϸ� */
    } else if ((app_sp >= 0x10000000) && (app_sp < 0x10010000)) {
        sp_valid = 1;  /* SRAM2 ��Χ�Ϸ� */
    }
    
    if (!sp_valid) {
        BLE_PRINTF("[JUMP] Ŀ�� 0x%08X ջ���Ƿ�:0x%08X, ����SRAM��Χ\r\n", addr, app_sp);
        return;
    }

    app_pc = *(volatile uint32_t *)(addr + 4);
    
    /* ��� Reset_Handler ��ַ�Ƿ��� Flash ��Χ�� */
    if ((app_pc < 0x08040000) || (app_pc >= 0x08100000)) {
        BLE_PRINTF("[JUMP] Ŀ�� 0x%08X PC�Ƿ�:0x%08X, ����Flash��Χ\r\n", addr, app_pc);
        return;
    }
    
    /* ��� Thumb λ (���λΪ 1) */
    if (!(app_pc & 1)) {
        BLE_PRINTF("[JUMP] Ŀ�� 0x%08X PC�Ƿ�:0x%08X, Thumbλδ��1\r\n", addr, app_pc);
        return;
    }
    
    BLE_PRINTF("[JUMP] ��ת 0x%08X SP=0x%08X PC=0x%08X\r\n", addr, app_sp, app_pc);

    /* �ر��ж� */
    __disable_irq();

    /* �ر� SysTick */
    SysTick->CTRL = 0;
    SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;
    
    /* ��� NVIC �ж� */
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* �����ж������� */
    SCB->VTOR = addr;

    /* ������ջָ�� */
    __set_MSP(app_sp);
    
    /* ʹ���ж� */
    __enable_irq();

    /* ��ת */
    ((void (*)(void))app_pc)();
}



static KeyHandle_TypeDef Key0;

static KeyHandle_TypeDef Key1;
static KeyHandle_TypeDef Key2;
static KeyHandle_TypeDef Key3;

static KeyHandle_TypeDef Key4;

 
 
 
void KeyP4_ClickedCallback(){   // KEY4: ���ȿ����л����̰���
	KeyNum > 0 ? (KeyNum = 0 ,Process_Command('h')) :  (KeyNum = 2 ,Process_Command('b'));

}

void KeyP4_LongPressedCallback(void){   // KEY4: ����3���л�����
  BEEP_Init();
	

	uint32_t current = Get_Current_Partition();
    uint32_t target;

    if(current == PARTITION_A_ADDR){
			
				GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 ��
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 ��
		delay_ms(300);
	GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 ��
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 ��
		delay_ms(300);
			GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 ��
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 ��
		delay_ms(300);
				
			
			
			
        target = PARTITION_B_ADDR;
        BLE_PRINTF("\r\n[KEY4] ����3��! A�� -> B��\r\n");
    }else{
			
			
			// A ����Ч��LED0 �� 1 �κ���ת
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 ��
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 ��
		delay_ms(500);
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 ��
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 ��
		delay_ms(500);
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 ��
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 ��
		delay_ms(500);
			
        target = PARTITION_A_ADDR;
        BLE_PRINTF("\r\n[KEY4] ����3��! B�� -> A��\r\n");
    }

    BLE_PRINTF("[KEY4] ��ǰ:0x%08X Ŀ��:0x%08X\r\n", current, target);
 
//		  GPIO_SetBits(GPIOF, GPIO_Pin_8);
//		  delay_ms(500);
//		  GPIO_ResetBits(GPIOF, GPIO_Pin_8);
//		  delay_ms(500);


 

		/* ��������λ�л�����: д��־�� RTC ���ݼĴ�����ϵͳ��λ,
		   Bootloader ��ȡ��־����ת, Ӳ����λȷ����������ɾ�(����
		   APP��ֱ����תʱ OLED����IIC��������/TIM/USART�������»�������) */
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
		PWR_BackupAccessCmd(ENABLE);
		RTC->BKP0R = (target == PARTITION_A_ADDR) ? OTA_UPGRADE_A : OTA_UPGRADE_B;
		BLE_PRINTF("[KEY4] ��������λ -> 0x%08X\r\n", target);
		delay_ms(50);  /* �ȴ����������� */
		NVIC_SystemReset();  /* ϵͳ��λ, ���᷵�� */
}

void Key3_ClickedCallback(){   // KEY3: �л�����
	Process_Command(CMD_BT_DISC);
	BT_Toggle();
	BLE_PRINTF("KEY3: �����������л�\r\n");
	
	
			GPIO_ResetBits(GPIOF, LED0_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOF, LED0_PIN);
	
	
}

void Key2_ClickedCallback(){   // KEY2: �л�WiFi
	Process_Command(CMD_WIFI_DISC);
	WiFi_Toggle();
	BLE_PRINTF("KEY2: WiFi�������л�\r\n");
	
	
			GPIO_ResetBits(GPIOF, LED1_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOF, LED1_PIN);
	
}

void Key1_ClickedCallback(){   // KEY1: ����
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
	BLE_PRINTF("KEY1:����\r\n");
	
		GPIO_ResetBits(GPIOE, LED2_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOE, LED2_PIN);
	
}

void Key0_ClickedCallback(){   // KEY0: ����
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
	BLE_PRINTF("KEY0:����\r\n");


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


void Key_scan(KeyHandle_TypeDef *Handle){   // ����ɨ��,���ִ����̰�,�������������ص�


		Handle->keycurrent = GPIO_ReadInputDataBit(Handle->GPIO_Port, Handle->GPIO_Pin) ;

		/* ���������� */
		if(Handle->keycurrent == 0)
		{
				Handle->press_cnt++;

				/* �ﵽ������ֵ��δ������ */
				if(Handle->press_cnt >= LONG_PRESS_3S &&
					 Handle->long_triggered == 0 &&
					 Handle->LongPressedCallback != NULL)
				{
						Handle->long_triggered = 1;
						Handle->LongPressedCallback();
				}
		}

		/* ������֣������أ� */
		if(Handle->keyprevious == 0 && Handle->keycurrent == 1)
		{
				/* ֻ�г���δ����ʱ�Ŵ����̰� */
				if(Handle->long_triggered == 0 && Handle->ClickedCallback != NULL)
				{
						Handle->ClickedCallback();
				}
				/* ���ֺ����� */
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
	Key4.LongPressedCallback = KeyP4_LongPressedCallback;  /* ����3���л����� */
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
