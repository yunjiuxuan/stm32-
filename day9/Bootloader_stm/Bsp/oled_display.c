#include "oled_display.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
#include "dsp_usart.h"
#include "version.h"

#ifndef BOOTLOADER_MODE
 
static conn_status_t s_bt_status   = STATUS_OFF;
static conn_status_t s_wifi_status = STATUS_OFF;
static uint8_t       s_gear        = 0;

/* ===== ????????????????????????? ===== */
static conn_status_t s_last_bt     = 0xFF;
static conn_status_t s_last_wifi   = 0xFF;
static uint8_t       s_last_gear   = 0xFF;
static uint8_t       s_last_person = 0xFF;
static uint8_t       s_last_temp   = 0xFF;
static int           s_last_dist   = -1;
/* ??????????\*/
static uint8_t       s_last_cd_state = 0xFF;
static uint32_t      s_last_cd_sec   = 0xFFFFFFFF;

/* ================================================================
 * ??????????????????K
 *   - ????
 *   - ???????§à????\???(??§»??????????????????K
 *   - ????"??????????,????????????§Ø??????
 * ================================================================ */
void OLED_Display_Init(void)
{
    OLED_Clear();

    /* --- ????logo(???????) --- */
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

		
		
		
    /* --- ?J?§à????q??????bemfa????) --- */
    OLED_ShowCHinese(0,  0, IDX_HAN_LAN);
    OLED_ShowCHinese(16, 0, IDX_HAN_YA);
    OLED_ShowString(32, 0, (u8 *)":", 16);
    OLED_ShowString(56, 0, (u8 *)" bemfa:", 16);

    /* --- ?J?§à????q??¦Ë??e?????\ --- */
    OLED_ShowCHinese(0,  3, IDX_HAN_DANG);
    OLED_ShowCHinese(16, 3, IDX_HAN_WEI);
    OLED_ShowString(32, 3, (u8 *)":", 16);
    OLED_ShowCHinese(113, 3, IDX_HAN_REN);

    /* --- ?J?? ???????"???"???,?????????x=0?????j---
     *     ???????????\???? */
    OLED_ShowCHinese(120, 6, IDX_HAN_DEGREE);

    /* ????"?????????? ???? Refresh ??¦Å???????????K*/
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
 * ???????????o????????????????£???§Õ??,???????return
 * ================================================================ */

/* ?????????? (40,0) 16x16 ???? */
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

/* WiFi?????? (113,0) 16x16 ???? */
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

/* ??¦Ë??????? (32,3) ???????; ??????????????¦Â???*/
static void Refresh_Gear(void)
{
    char buf[20];
    if(s_gear == s_last_gear) return;
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), ":%d  ", s_gear);
    OLED_ShowString(32, 3, (u8 *)buf, 16);
    s_last_gear = s_gear;
}

/* ?????????????: (60~112,3)
 *   ????: ????(2? + "cm" + "?c
 *   ????: ?????????? + "?J
 *   ????§Ø??????¦Ë????(????????????a */
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
        /* ??????????,???????????????*/
        if(!daojishi_state)
            Process_Command(CMD_GEAR_OFF);
    }
    s_last_person = has_person;
    s_last_dist   = curr_dist;
}

/* ?????????? (100,6) 2¦Ë???\*/
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

/* ???????????? (0,6) ??? HH:MM:SS
 *   - daojishi_state=1: ????????J
 *   - daojishi_state=0: ??? 00:00:00
 *   - ???????????????£?????? */
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
 * ???????????????K
 *   - ?????mOLED_Clear(),???????§à??????
 *   - ??????????§Ø?:"??????????????m
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

#endif /* BOOTLOADER_MODE */

/* ===== OTA ===== */
static uint8_t s_last_ota_percent = 0xFF;

void OLED_OTA_ShowProgress(uint8_t percent)
{
    char buf[8];

    if (s_last_ota_percent == 0xFF)
    {
        OLED_ShowCHinese(0,  6, IDX_HAN_SHENG);
        OLED_ShowCHinese(16, 6, IDX_HAN_JI);
        OLED_ShowCHinese(32, 6, IDX_HAN_ZHONG);
        OLED_ShowString(48, 6, (u8 *)"...  ", 16);
    }

    if (percent != s_last_ota_percent)
    {
        snprintf(buf, sizeof(buf), "%3d%%", percent);
        OLED_ShowString(72, 6, (u8 *)buf, 16);
        s_last_ota_percent = percent;
    }
}

void OLED_OTA_ShowSuccess(void)
{
    OLED_ShowString(0, 6, (u8 *)"                ", 16);

    OLED_ShowCHinese(0,  6, IDX_HAN_SHENG);
    OLED_ShowCHinese(16, 6, IDX_HAN_JI);
    OLED_ShowCHinese(32, 6, IDX_HAN_WAN);
    OLED_ShowCHinese(48, 6, IDX_HAN_CHENG);

    s_last_ota_percent = 0xFF;
}

void OLED_OTA_ShowFail(void)
{
    OLED_ShowString(0, 4, (u8 *)"                ", 16);
    OLED_ShowString(0, 6, (u8 *)"                ", 16);

    OLED_ShowCHinese(0,  4, IDX_HAN_SHENG);
    OLED_ShowCHinese(16, 4, IDX_HAN_JI);
    OLED_ShowCHinese(32, 4, IDX_HAN_SHI);
    OLED_ShowCHinese(48, 4, IDX_HAN_BAI);

    OLED_ShowCHinese(0,  6, IDX_HAN_TUI);
    OLED_ShowCHinese(16, 6, IDX_HAN_HUI);
    OLED_ShowCHinese(32, 6, IDX_HAN_YUAN);
    OLED_ShowCHinese(48, 6, IDX_HAN_BAN);
    OLED_ShowCHinese(64, 6, IDX_HAN_BEN);

    s_last_ota_percent = 0xFF;
}
