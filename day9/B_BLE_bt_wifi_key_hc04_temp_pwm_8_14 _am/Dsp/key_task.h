 #include <stm32f4xx.h>
 #include "key.h"
typedef struct{


	int keynum;

	 uint8_t	keyprevious;
	 uint8_t	keycurrent;

	GPIO_TypeDef *GPIO_Port;
	uint16_t  GPIO_Pin;


	void (*ClickedCallback)(void);      /* 短按回调（松手触发） */
	void (*LongPressedCallback)(void);  /* 长按回调（按住达到阈值触发） */
	uint16_t press_cnt;                 /* 按下持续计数器 */
	uint8_t  long_triggered;            /* 长按是否已触发标志 */



} KeyHandle_TypeDef;

 
  void key_task(void *pvParameters);

 void Keytask_Init(KeyHandle_TypeDef *Handle);
 void Key_scan(KeyHandle_TypeDef *Handle);
 
 
 
extern  uint8_t	keyprevious  ;
extern	 uint8_t	keycurrent  ;
