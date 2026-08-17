#ifndef __OLED_DISPLAY_H
#define __OLED_DISPLAY_H

#include "stm32f4xx.h"

// 连接状态：关/开/已连接
typedef enum {
    STATUS_OFF       = 0,
    STATUS_ON        = 1,
    STATUS_CONNECTED = 2
} conn_status_t;

void OLED_Display_Init(void);
void OLED_SetBTStatus(conn_status_t status);
void OLED_SetWiFiStatus(conn_status_t status);
void OLED_SetGear(uint8_t gear);
void OLED_Display_Refresh(uint32_t elapsed_sec, int curr_dist,uint8_t has_person, uint8_t temp);

// 汉字在Hzk数组中的索引
#define IDX_HAN_LAN   14
#define IDX_HAN_YA    15
#define IDX_HAN_KAI   16
#define IDX_HAN_GUAN  17
#define IDX_HAN_SHI   18
#define IDX_HAN_JIAN  19
#define IDX_HAN_DANG  20
#define IDX_HAN_WEI   21
#define IDX_HAN_XIAO  22
#define IDX_HAN_FENG  23
#define IDX_HAN_SHAN  24
#define IDX_HAN_GOU   25
#define IDX_HAN_YOU   26
#define IDX_HAN_REN   27
#define IDX_HAN_WU    28
#define IDX_HAN_DEGREE 29
#define IDX_HAN_GANG   30

#endif //__OLED_DISPLAY_H
