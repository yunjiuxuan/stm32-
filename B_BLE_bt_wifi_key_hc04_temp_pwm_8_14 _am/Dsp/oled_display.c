#include "oled_display.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
#include "dsp_usart.h"
#include "version.h"
 
static conn_status_t s_bt_status   = STATUS_OFF;
static conn_status_t s_wifi_status = STATUS_OFF;
static uint8_t       s_gear        = 0;

/* ===== 记录上次显示的状怿用于局部刷新对毿 ===== */
static conn_status_t s_last_bt     = 0xFF;
static conn_status_t s_last_wifi   = 0xFF;
static uint8_t       s_last_gear   = 0xFF;
static uint8_t       s_last_person = 0xFF;
static uint8_t       s_last_temp   = 0xFF;
static int           s_last_dist   = -1;
/* 倒计时刷新缓孿*/
static uint8_t       s_last_cd_state = 0xFF;
static uint32_t      s_last_cd_sec   = 0xFFFFFFFF;

/* ================================================================
 * 初始化显示界靿只调用一欿
 *   - 清屏
 *   - 画好所有静态文孿标签(这些内容永远不会叿只画这一欿
 *   - 重置"上次状怿缓存,强制首次刷新所有动态区埿
 * ================================================================ */
void OLED_Display_Init(void)
{
    OLED_Clear();

    /* --- 开机logo(保持原样) --- */
    OLED_ShowString(20, 3, (u8 *)"stm32", 16);

    /* version info at bottom-right (small 6x8 font, y=7 last page) */
    {
        char ver_buf[16];
        const char *nm = g_version.name;
        snprintf(ver_buf, sizeof(ver_buf), "%c V%d.%d",
                 nm[strlen(nm)-1], g_version.ver_major, g_version.ver_minor);
        OLED_ShowString(72, 7, (u8 *)ver_buf, 8);
    }
    OLED_ShowCHinese(60, 3, IDX_HAN_XIAO);
    OLED_ShowCHinese(76, 3, IDX_HAN_FENG);
    OLED_ShowCHinese(92, 3, IDX_HAN_SHAN);

		  delay_us(3000);
		
		 OLED_Clear();

		
		
		
    /* --- 笿行静态标筿蓝牙、bemfa文字) --- */
    OLED_ShowCHinese(0,  0, IDX_HAN_LAN);
    OLED_ShowCHinese(16, 0, IDX_HAN_YA);
    OLED_ShowString(32, 0, (u8 *)":", 16);
    OLED_ShowString(56, 0, (u8 *)" bemfa:", 16);

    /* --- 笿行静态标筿档位、冒号?亿孿 --- */
    OLED_ShowCHinese(0,  3, IDX_HAN_DANG);
    OLED_ShowCHinese(16, 3, IDX_HAN_WEI);
    OLED_ShowString(32, 3, (u8 *)":", 16);
    OLED_ShowCHinese(113, 3, IDX_HAN_REN);

    /* --- 笿衿 不再显示"时间"标签,倒计时直接从x=0开始显礿---
     *     只保留温庿庿孿静怿 */
    OLED_ShowCHinese(120, 6, IDX_HAN_DEGREE);

    /* 重置"上次状怿缓存 ?保证 Refresh 首次调用全部执行一欿*/
    s_last_bt     = 0xFF;
    s_last_wifi   = 0xFF;
    s_last_gear   = 0xFF;
    s_last_person = 0xFF;
    s_last_temp   = 0xFF;
    s_last_dist   = -1;
    s_last_cd_state = 0xFF;
    s_last_cd_sec   = 0xFFFFFFFF;
}

void OLED_SetBTStatus(conn_status_t status)   { s_bt_status   = status; }
void OLED_SetWiFiStatus(conn_status_t status) { s_wifi_status = status; }
void OLED_SetGear(uint8_t gear)               { s_gear        = gear; }

/* ================================================================
 * 各区域独立刷新函敿—?只有数据真的变化了才写屏,否则直接return
 * ================================================================ */

/* 蓝牙状态区埿 (40,0) 16x16 汉字 */
static void Refresh_BT(void)
{
    if(s_bt_status == s_last_bt) return;
    if(s_bt_status == STATUS_CONNECTED)
        OLED_ShowCHinese(40, 0, IDX_HAN_GOU);
    else if(s_bt_status == STATUS_ON)
        OLED_ShowCHinese(40, 0, IDX_HAN_KAI);
    else
        OLED_ShowCHinese(40, 0, IDX_HAN_GUAN);
    s_last_bt = s_bt_status;
}

/* WiFi状态区埿 (113,0) 16x16 汉字 */
static void Refresh_WiFi(void)
{
    if(s_wifi_status == s_last_wifi) return;
    if(s_wifi_status == STATUS_CONNECTED)
        OLED_ShowCHinese(113, 0, IDX_HAN_GOU);
    else if(s_wifi_status == STATUS_ON)
        OLED_ShowCHinese(113, 0, IDX_HAN_KAI);
    else
        OLED_ShowCHinese(113, 0, IDX_HAN_GUAN);
    s_last_wifi = s_wifi_status;
}

/* 档位数值区埿 (32,3) 数字字符; 后面留空格擦除上次残畿*/
static void Refresh_Gear(void)
{
    char buf[20];
    if(s_gear == s_last_gear) return;
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), ":%d  ", s_gear);
    OLED_ShowString(32, 3, (u8 *)buf, 16);
    s_last_gear = s_gear;
}

/* 人员检浿距离区域: (60~112,3)
 *   有人: 距离(2使 + "cm" + "朿
 *   无人: 空格擦除残留 + "旿
 *   温度判断触发档位命令(和原逻辑保持一臿 */
static void Refresh_Person(int curr_dist, uint8_t has_person, uint8_t temp)
{
    if(has_person == s_last_person && curr_dist == s_last_dist) return;

    if(has_person)
    {
			
			if (curr_dist >35   ) {
				 OLED_ShowString(60, 3, (u8 *)"    ", 16);
				  OLED_ShowCHinese(80, 3, IDX_HAN_GANG);
			}
			else{
				OLED_ShowNum(60, 3, curr_dist, 2, 16);
        OLED_ShowString(75, 3, (u8 *)"cm  ", 16);
        OLED_ShowCHinese(96, 3, IDX_HAN_YOU);
        if(temp > 28)
            Process_Command(CMD_GEAR_2);
			}
			
      
    }
    else
    {
        OLED_ShowString(60, 3, (u8 *)"     ", 16);
        OLED_ShowCHinese(96, 3, IDX_HAN_WU);
        /* 倒计时开启时,即便无人也不关风承*/
        if(!daojishi_state)
            Process_Command(CMD_GEAR_OFF);
    }
    s_last_person = has_person;
    s_last_dist   = curr_dist;
}

/* 温度数值区埿 (100,6) 2位数孿*/
static void Refresh_Temp(uint8_t temp)
{
    if(temp == s_last_temp) return;
	if  (  temp == 0  ) {
		 OLED_ShowNum(100, 6, s_last_temp, 2, 16);
	}else{
		
    OLED_ShowNum(100, 6, temp, 2, 16);
		 s_last_temp = temp;
		
	} 
   
}

/* 倒计时显示区埿 (0,6) 格式 HH:MM:SS
 *   - daojishi_state=1: 显示剩余倒计旿
 *   - daojishi_state=0: 显示 00:00:00
 *   - 只有状态或剩余秒数变化时才刷新 */
static void Refresh_Countdown(void)
{
    char buf[16];
    uint32_t h, m, s;

    if(daojishi_state == s_last_cd_state && daojishi_remain_sec == s_last_cd_sec)
        return;

    if(daojishi_state && daojishi_remain_sec > 0)
    {
        h = daojishi_remain_sec / 3600;
        m = (daojishi_remain_sec % 3600) / 60;
        s = daojishi_remain_sec % 60;
    }
    else
    {
        h = m = s = 0;
    }
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    OLED_ShowString(0, 6, (u8 *)buf, 16);

    s_last_cd_state = daojishi_state;
    s_last_cd_sec   = daojishi_remain_sec;
}

/* ================================================================
 * 主刷新入叿每秒调用一欿
 *   - 不调甿OLED_Clear(),保留所有静态内宿
 *   - 各区域逐个判断:"没变就跳迿变了才重甿
 * ================================================================ */
void OLED_Display_Refresh(uint32_t elapsed_sec, int curr_dist, uint8_t has_person, uint8_t temp)
{
    Refresh_BT();
    Refresh_WiFi();
    Refresh_Gear();
    Refresh_Person(curr_dist, has_person, temp);
    Refresh_Temp(temp);
    Refresh_Countdown();
}
