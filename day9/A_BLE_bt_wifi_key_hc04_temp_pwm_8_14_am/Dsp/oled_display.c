#include "oled_display.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
#include "dsp_usart.h"
#include "version.h"



//内部状态变�?
static conn_status_t s_bt_status   = STATUS_OFF;
static conn_status_t s_wifi_status = STATUS_OFF;
static uint8_t       s_gear        = 0;  //风扇档位�?=停止�?

//初始化显示界�?
void OLED_Display_Init(void)
{
    OLED_Clear();

    OLED_ShowString(20, 3, (u8 *)"stm32", 16);

    /* version info at bottom-right (small 6x8 font, y=7 last page) */
    {
        char ver_buf[16];
        const char *nm = g_version.name;
        snprintf(ver_buf, sizeof(ver_buf), "%c V%d.%d",
                 nm[strlen(nm)-1], g_version.ver_major, g_version.ver_minor);
        OLED_ShowString(72, 7, (u8 *)ver_buf, 8);
    }
    OLED_ShowCHinese(60, 3, IDX_HAN_XIAO);  //�?
    OLED_ShowCHinese(76, 3, IDX_HAN_FENG);  //�?
    OLED_ShowCHinese(92, 3, IDX_HAN_SHAN);  //�?
}

//设置蓝牙连接状�?
void OLED_SetBTStatus(conn_status_t status)
{
    s_bt_status = status;
}

//设置WiFi连接状�?
void OLED_SetWiFiStatus(conn_status_t status)
{
    s_wifi_status = status;
}

//设置风扇档位�?=停止�?~n=对应档位�?
void OLED_SetGear(uint8_t gear)
{
    s_gear = gear;
}
 
 

//格式化运行时�?
//< 60s   : "XXs"
//< 1h    : "XX:XX" (�?�?
//< 1day  : "XX:XX:XX" (�?�?�?
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
//has_person/temp �?app_task1 采集后直接传入，无需setter
void OLED_Display_Refresh(uint32_t elapsed_sec, int curr_dist,uint8_t has_person, uint8_t temp)
{
    char line_buf[20];
    OLED_Clear();

    //===== �?行：蓝牙:�?开/�?  bemfa:�?开/OK =====
    OLED_ShowCHinese(0,  0, IDX_HAN_LAN);   //�?
    OLED_ShowCHinese(16, 0, IDX_HAN_YA);    //�?
    OLED_ShowString(32, 0, (u8 *)":", 16);

    //蓝牙状态三态：OFF=�? ON=开, CONNECTED=�?
    if(s_bt_status == STATUS_CONNECTED)
        OLED_ShowCHinese(40, 0, IDX_HAN_GOU);    //�?
    else if(s_bt_status == STATUS_ON)
        OLED_ShowCHinese(40, 0, IDX_HAN_KAI);   //开
    else
        OLED_ShowCHinese(40, 0, IDX_HAN_GUAN);  //�?

    OLED_ShowString(56, 0, (u8 *)" bemfa:", 16);

     //WiFi状态三态：OFF=�? ON=开, CONNECTED=�?
    if(s_wifi_status == STATUS_CONNECTED)
        OLED_ShowCHinese(113, 0, IDX_HAN_GOU);   //�?
    else if(s_wifi_status == STATUS_ON)
        OLED_ShowCHinese(113, 0, IDX_HAN_KAI);   //开
    else
        OLED_ShowCHinese(113, 0, IDX_HAN_GUAN);  //�?

    //===== �?行：档位:n�?=====
    OLED_ShowCHinese(0,  3, IDX_HAN_DANG);  //�?
    OLED_ShowCHinese(16, 3, IDX_HAN_WEI);   //�?
    memset(line_buf, 0, sizeof(line_buf));
    snprintf(line_buf, sizeof(line_buf), ":%d", s_gear);
    OLED_ShowString(32, 3, (u8 *)line_buf, 16);

		
		
    //===== �?行续：有�?无人（直接使用传入参数）=====
    if(has_person)
    {
			if(curr_dist < 30 ){
				
				OLED_ShowNum(60, 3,curr_dist,2,16);
			
			 OLED_ShowString(75, 3, (u8 *)"cm", 16);
			}else{
				
			 OLED_ShowCHinese(80, 3, IDX_HAN_GANG);  //�?
				
			}
        OLED_ShowCHinese(96, 3, IDX_HAN_YOU);  //�?
    }
    else
    {
        OLED_ShowCHinese(96, 3, IDX_HAN_WU);   //�?
    }
    OLED_ShowCHinese(113, 3, IDX_HAN_REN);  //�?

    OLED_ShowNum(100, 6, temp, 2, 16);  //温度
    OLED_ShowCHinese(120, 6, IDX_HAN_DEGREE);  //° 度数符号
	 
		
    //===== �?行：时间:XX:XX:XX =====
    OLED_ShowCHinese(0,  6, IDX_HAN_SHI);   //�?
    OLED_ShowCHinese(16, 6, IDX_HAN_JIAN);  //�?
    OLED_ShowString(32, 6, (u8 *)":", 16);
    memset(line_buf, 0, sizeof(line_buf));
    Format_Time(elapsed_sec, line_buf, sizeof(line_buf));
    OLED_ShowString(48, 6, (u8 *)(line_buf + 6), 16);
}
