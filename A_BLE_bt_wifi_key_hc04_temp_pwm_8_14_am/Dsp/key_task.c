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

/* ===== ·ÖÇø¶¨Òå (Flash ÈýµÈ·Ö, A=B=384KB) ===== */
#define PARTITION_A_ADDR  0x08040000   /* AÇøÆðÊ¼µØÖ· (ÉÈÇø 6-8, 384KB) */
#define PARTITION_B_ADDR  0x080A0000   /* BÇøÆðÊ¼µØÖ· (ÉÈÇø 9-11, 384KB) */

/* ===== ·ÖÇøÇÐ»»ÇëÇó±êÖ¾ (Ð´Èë RTC ±¸·Ý¼Ä´æÆ÷ BKP0R, Èí¸´Î»ºóÓÉ Bootloader ¶ÁÈ¡) ===== */
#define JUMP_FLAG_TO_A   0xA5A50001
#define JUMP_FLAG_TO_B   0xA5A50002
#define OTA_UPGRADE_A    0xA5A50003
#define OTA_UPGRADE_B    0xA5A50004
#define LONG_PRESS_3S     200          /* 3Ãë: 200´Î ¡Á 15ms */

 
/* ===== èŽ·å–å½“å‰è¿è¡Œåˆ†åŒºåœ°å€ ===== */
/* noinline: é˜²æ­¢Keil -O2/-O3å†…è”ä¼˜åŒ–å¯¼è‡´ (uint32_t)Get_Current_Partition å–åœ°å€å¤±æ•ˆ */
__attribute__((noinline)) static uint32_t Get_Current_Partition(void)
{
    uint32_t addr = (uint32_t)Get_Current_Partition;
    if(addr >= PARTITION_B_ADDR)
        return PARTITION_B_ADDR;  /* BåŒº */
    else
        return PARTITION_A_ADDR;  /* AåŒº */
}
/* ===== Ìø×ªµ½Ö¸¶¨·ÖÇø ===== */
static void Jump_To_Partition(uint32_t addr)
{





	
    uint32_t app_sp;
    uint32_t app_pc;
    int sp_valid = 0;

    /* ¼ì²éÕ»¶¥ÖµÊÇ·ñºÏ·¨ (STM32F407ZEÓÐÁ½¸öSRAMÇøÓò) */
    /* SRAM1: 0x20000000 - 0x2001FFFF (128KB) */
    /* SRAM2: 0x10000000 - 0x10000FFFF (64KB) */
    app_sp = *(volatile uint32_t *)addr;
    
    if ((app_sp >= 0x20000000) && (app_sp < 0x20020000)) {
        sp_valid = 1;  /* SRAM1 ·¶Î§ºÏ·¨ */
    } else if ((app_sp >= 0x10000000) && (app_sp < 0x10010000)) {
        sp_valid = 1;  /* SRAM2 ·¶Î§ºÏ·¨ */
    }
    
    if (!sp_valid) {
        BLE_PRINTF("[JUMP] Ä¿±ê 0x%08X Õ»¶¥·Ç·¨:0x%08X, ²»ÔÚSRAM·¶Î§\r\n", addr, app_sp);
        return;
    }

    app_pc = *(volatile uint32_t *)(addr + 4);
    
    /* ¼ì²é Reset_Handler µØÖ·ÊÇ·ñÔÚ Flash ·¶Î§ÄÚ */
    if ((app_pc < 0x08040000) || (app_pc >= 0x08100000)) {
        BLE_PRINTF("[JUMP] Ä¿±ê 0x%08X PC·Ç·¨:0x%08X, ²»ÔÚFlash·¶Î§\r\n", addr, app_pc);
        return;
    }
    
    /* ¼ì²é Thumb Î» (×îµÍÎ»Îª 1) */
    if (!(app_pc & 1)) {
        BLE_PRINTF("[JUMP] Ä¿±ê 0x%08X PC·Ç·¨:0x%08X, ThumbÎ»Î´ÖÃ1\r\n", addr, app_pc);
        return;
    }
    
    BLE_PRINTF("[JUMP] Ìø×ª 0x%08X SP=0x%08X PC=0x%08X\r\n", addr, app_sp, app_pc);

    /* ¹Ø±ÕÖÐ¶Ï */
    __disable_irq();

    /* ¹Ø±Õ SysTick */
    SysTick->CTRL = 0;
    SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;
    
    /* Çå³ý NVIC ÖÐ¶Ï */
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* ÉèÖÃÖÐ¶ÏÏòÁ¿±í */
    SCB->VTOR = addr;

    /* ÉèÖÃÖ÷Õ»Ö¸Õë */
    __set_MSP(app_sp);
    
    /* Ê¹ÄÜÖÐ¶Ï */
    __enable_irq();

    /* Ìø×ª */
    ((void (*)(void))app_pc)();
}



static KeyHandle_TypeDef Key0;

static KeyHandle_TypeDef Key1;
static KeyHandle_TypeDef Key2;
static KeyHandle_TypeDef Key3;

static KeyHandle_TypeDef Key4;

 
 
 
void KeyP4_ClickedCallback(){   // KEY4: ·çÉÈ¿ª¹ØÇÐ»»£¨¶Ì°´£©
	KeyNum > 0 ? (KeyNum = 0 ,Process_Command('0')) :  (KeyNum = 2 ,Process_Command('b'));

}

void KeyP4_LongPressedCallback(void){   // KEY4: ³¤°´3ÃëÇÐ»»ÉÈÇø
  BEEP_Init();
	

	uint32_t current = Get_Current_Partition();
    uint32_t target;

    if(current == PARTITION_A_ADDR){
			
				GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 ÁÁ
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 Ãð
		delay_ms(300);
	GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 ÁÁ
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 Ãð
		delay_ms(300);
			GPIO_ResetBits(GPIOE, GPIO_Pin_13);  // LED0 ÁÁ
		delay_ms(500);
		GPIO_SetBits(GPIOE, GPIO_Pin_13);    // LED0 Ãð
		delay_ms(300);
				
			
			
			
        target = PARTITION_B_ADDR;
        BLE_PRINTF("\r\n[KEY4] ³¤°´3Ãë! AÇø -> BÇø\r\n");
    }else{
			
			
			// A ÇøÓÐÐ§£¬LED0 ÉÁ 1 ´ÎºóÌø×ª
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 ÁÁ
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 Ãð
		delay_ms(500);
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 ÁÁ
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 Ãð
		delay_ms(500);
		GPIO_ResetBits(GPIOF, GPIO_Pin_10);  // LED0 ÁÁ
		delay_ms(500);
		GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED0 Ãð
		delay_ms(500);
			
        target = PARTITION_A_ADDR;
        BLE_PRINTF("\r\n[KEY4] ³¤°´3Ãë! BÇø -> AÇø\r\n");
    }

    BLE_PRINTF("[KEY4] µ±Ç°:0x%08X Ä¿±ê:0x%08X\r\n", current, target);
 
//		  GPIO_SetBits(GPIOF, GPIO_Pin_8);
//		  delay_ms(500);
//		  GPIO_ResetBits(GPIOF, GPIO_Pin_8);
//		  delay_ms(500);


 

		/* ´¥·¢Èí¸´Î»ÇÐ»»·ÖÇø: Ð´±êÖ¾µ½ RTC ±¸·Ý¼Ä´æÆ÷ºóÏµÍ³¸´Î»,
		   Bootloader ¶ÁÈ¡±êÖ¾²¢Ìø×ª, Ó²¼þ¸´Î»È·±£ËùÓÐÍâÉè¸É¾»(±ÜÃâ
		   APP¼äÖ±½ÓÌø×ªÊ± OLEDÈí¼þIIC×ÜÏßËÀËø/TIM/USART²ÐÁôµ¼ÖÂ»¨ÆÁ¿¨ËÀ) */
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
		PWR_BackupAccessCmd(ENABLE);
		RTC->BKP0R = (target == PARTITION_A_ADDR) ? OTA_UPGRADE_A : OTA_UPGRADE_B;
		BLE_PRINTF("[KEY4] ´¥·¢Èí¸´Î» -> 0x%08X\r\n", target);
		delay_ms(50);  /* µÈ´ý´®¿ÚÊä³öÍê³É */
		NVIC_SystemReset();  /* ÏµÍ³¸´Î», ²»»á·µ»Ø */
}

void Key3_ClickedCallback(){   // KEY3: ÇÐ»»À¶ÑÀ
	Process_Command(CMD_BT_DISC);
	BT_Toggle();
	BLE_PRINTF("KEY3: À¶ÑÀ¿ª¹ØÒÑÇÐ»»\r\n");
	
	
			GPIO_ResetBits(GPIOF, LED0_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOF, LED0_PIN);
	
	
}

void Key2_ClickedCallback(){   // KEY2: ÇÐ»»WiFi
	Process_Command(CMD_WIFI_DISC);
	WiFi_Toggle();
	BLE_PRINTF("KEY2: WiFi¿ª¹ØÒÑÇÐ»»\r\n");
	
	
			GPIO_ResetBits(GPIOF, LED1_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOF, LED1_PIN);
	
}

void Key1_ClickedCallback(){   // KEY1: ½µµµ
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
	BLE_PRINTF("KEY1:½µµµ\r\n");
	
		GPIO_ResetBits(GPIOE, LED2_PIN);
	
	vTaskDelay(pdMS_TO_TICKS(200));
	
GPIO_SetBits(GPIOE, LED2_PIN);
	
}

void Key0_ClickedCallback(){   // KEY0: Éýµµ
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
	BLE_PRINTF("KEY0:Éýµµ\r\n");


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


void Key_scan(KeyHandle_TypeDef *Handle){   // °´¼üÉ¨Ãè,ËÉÊÖ´¥·¢¶Ì°´,³¤°´´¥·¢³¤°´»Øµ÷


		Handle->keycurrent = GPIO_ReadInputDataBit(Handle->GPIO_Port, Handle->GPIO_Pin) ;

		/* °´¼ü°´ÏÂÖÐ */
		if(Handle->keycurrent == 0)
		{
				Handle->press_cnt++;

				/* ´ïµ½³¤°´ãÐÖµÇÒÎ´´¥·¢¹ý */
				if(Handle->press_cnt >= LONG_PRESS_3S &&
					 Handle->long_triggered == 0 &&
					 Handle->LongPressedCallback != NULL)
				{
						Handle->long_triggered = 1;
						Handle->LongPressedCallback();
				}
		}

		/* ¼ì²âËÉÊÖ£¨ÉÏÉýÑØ£© */
		if(Handle->keyprevious == 0 && Handle->keycurrent == 1)
		{
				/* Ö»ÓÐ³¤°´Î´´¥·¢Ê±²Å´¥·¢¶Ì°´ */
				if(Handle->long_triggered == 0 && Handle->ClickedCallback != NULL)
				{
						Handle->ClickedCallback();
				}
				/* ËÉÊÖºóÇåÁã */
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
	Key4.LongPressedCallback = KeyP4_LongPressedCallback;  /* ³¤°´3ÃëÇÐ»»ÉÈÇø */
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
