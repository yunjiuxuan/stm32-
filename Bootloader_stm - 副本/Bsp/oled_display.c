#include "oled_display.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>

#include "dsp_usart.h"
//内部状态变量
static conn_status_t s_bt_status   = STATUS_OFF;
static conn_status_t s_wifi_status = STATUS_OFF;
static uint8_t       s_gear        = 0;  //风扇档位（0=停止）

//初始化显示界面
void OLED_Display_Init(void)
{
    OLED_Clear();

    OLED_ShowString(0, 4, (u8 *)"stm32", 16);
    OLED_ShowCHinese(40, 4, IDX_HAN_XIAO);  //小
    OLED_ShowCHinese(56, 4, IDX_HAN_FENG);  //风
    OLED_ShowCHinese(72, 4, IDX_HAN_SHAN);  //扇
}

//设置蓝牙连接状态
void OLED_SetBTStatus(conn_status_t status)
{
    s_bt_status = status;
}

//设置WiFi连接状态
void OLED_SetWiFiStatus(conn_status_t status)
{
    s_wifi_status = status;
}

//设置风扇档位（0=停止，1~n=对应档位）
void OLED_SetGear(uint8_t gear)
{
    s_gear = gear;
}

//格式化运行时间
//< 60s   : "XXs"
//< 1h    : "XX:XX" (分:秒)
//< 1day  : "XX:XX:XX" (时:分:秒)
//>= 1day : "Xd XX:XX:XX"
static void Format_Time(uint32_t sec, char *buf, uint16_t buf_size)
{
    uint32_t days  = sec / 86400;
    uint32_t hours = (sec % 86400) / 3600;
    uint32_t mins  = (sec % 3600) / 60;
    uint32_t secs  = sec % 60;

    if(sec < 60)
    {
        snprintf(buf, buf_size, "Time: %lus", (unsigned long)secs);
    }
    else if(sec < 3600)
    {
        snprintf(buf, buf_size, "Time: %02lu:%02lu", (unsigned long)mins, (unsigned long)secs);
    }
    else if(sec < 86400)
    {
        snprintf(buf, buf_size, "Time: %02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
    }
    else
    {
        snprintf(buf, buf_size, "Time: %lud %02lu:%02lu", (unsigned long)days, (unsigned long)hours, (unsigned long)mins);
    }
}

//刷新OLED显示（每秒调用一次）
void OLED_Display_Refresh(uint32_t elapsed_sec)
{
    char line_buf[20];
    OLED_Clear();

    //===== 第1行：蓝牙:关/开/OK   bemfa:关/开/OK =====
    OLED_ShowCHinese(0,  0, IDX_HAN_LAN);   //蓝
    OLED_ShowCHinese(16, 0, IDX_HAN_YA);    //牙
    OLED_ShowString(32, 0, (u8 *)":", 16);

    //蓝牙状态三态：OFF=关, ON=开, CONNECTED=OK
    if(s_bt_status == STATUS_CONNECTED)
        OLED_ShowString(40, 0, (u8 *)"OK", 16);
    else if(s_bt_status == STATUS_ON)
        OLED_ShowCHinese(40, 0, IDX_HAN_KAI);   //开
    else
        OLED_ShowCHinese(40, 0, IDX_HAN_GUAN);  //关

    OLED_ShowString(56, 0, (u8 *)" bemfa:", 16);

    //WiFi状态三态：OFF=关, ON=开, CONNECTED=OK
    if(s_wifi_status == STATUS_CONNECTED)
        OLED_ShowString(113, 0, (u8 *)"OK", 16);
    else if(s_wifi_status == STATUS_ON)
        OLED_ShowCHinese(113, 0, IDX_HAN_KAI);   //开
    else
        OLED_ShowCHinese(113, 0, IDX_HAN_GUAN);  //关

    //===== 第2行：档位:n档 =====
    OLED_ShowCHinese(0,  3, IDX_HAN_DANG);  //档
    OLED_ShowCHinese(16, 3, IDX_HAN_WEI);   //位
    memset(line_buf, 0, sizeof(line_buf));
    snprintf(line_buf, sizeof(line_buf), ":%d", s_gear);
    OLED_ShowString(32, 3, (u8 *)line_buf, 16);

    //===== 第4行：时间:XX:XX:XX =====
    OLED_ShowCHinese(0,  6, IDX_HAN_SHI);   //时
    OLED_ShowCHinese(16, 6, IDX_HAN_JIAN);  //间
    OLED_ShowString(32, 6, (u8 *)":", 16);
    memset(line_buf, 0, sizeof(line_buf));
    Format_Time(elapsed_sec, line_buf, sizeof(line_buf));
    OLED_ShowString(48, 6, (u8 *)(line_buf + 6), 16);
}
