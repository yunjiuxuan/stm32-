#ifndef __OLED_DISPLAY_H
#define __OLED_DISPLAY_H

#include "stm32f4xx.h"

//连接状态枚举
//STATUS_OFF        : 未开启  -> '关'
//STATUS_ON         : 开启中  -> '开'
//STATUS_CONNECTED  : 连接成功 -> 'OK'
typedef enum {
    STATUS_OFF       = 0,
    STATUS_ON        = 1,
    STATUS_CONNECTED = 2
} conn_status_t;

//初始化显示界面
void OLED_Display_Init(void);

//设置蓝牙连接状态
void OLED_SetBTStatus(conn_status_t status);

//设置WiFi连接状态
void OLED_SetWiFiStatus(conn_status_t status);

//设置风扇档位（0=停止，1~n=对应档位）
void OLED_SetGear(uint8_t gear);

//刷新OLED显示（每秒调用一次）
//参数: elapsed_sec - 从启动开始的秒数
void OLED_Display_Refresh(uint32_t elapsed_sec);

//汉字在Hzk数组中的索引（与oledfont.h保持一致）
#define IDX_HAN_LAN   14  // 蓝
#define IDX_HAN_YA    15  // 牙
#define IDX_HAN_KAI   16  // 开
#define IDX_HAN_GUAN  17  // 关
#define IDX_HAN_SHI   18  // 时
#define IDX_HAN_JIAN  19  // 间
#define IDX_HAN_DANG  20  // 档
#define IDX_HAN_WEI   21  // 位
#define IDX_HAN_XIAO  22  // 小
#define IDX_HAN_FENG  23  // 风
#define IDX_HAN_SHAN  24  // 扇

#endif //__OLED_DISPLAY_H
