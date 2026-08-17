#include "oled_display.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>

#include "dsp_usart.h"
#include "version.h"

/* ===== 普通模式状态变量 ===== */
static conn_status_t s_bt_status   = STATUS_OFF;
static conn_status_t s_wifi_status = STATUS_OFF;
static uint8_t       s_gear        = 0;

/* ===== OTA 3行布局状态变量 ===== */
static uint8_t      s_ota_mode        = 0;   /* =1 已进入OTA界面 */
static ota_part_t   s_ota_part       = OTA_PART_B;
static ota_stage_t  s_ota_stage      = OTA_STAGE_IDLE;
static ota_stage_t  s_ota_final      = OTA_STAGE_IDLE;
static uint32_t     s_ota_err        = 0;
static uint32_t     s_ota_cd         = 0;

/* 防重绘: last值 */
static ota_stage_t  s_last_stage     = (ota_stage_t)0xFF;
static uint32_t     s_last_pct       = 0xFFFFFFFFU;
static uint32_t     s_last_done      = 0xFFFFFFFFU;
static ota_stage_t  s_last_final     = (ota_stage_t)0xFF;
static uint32_t     s_last_err       = 0xFFFFFFFFU;
static uint32_t     s_last_cd        = 0xFFFFFFFFU;
static uint32_t     s_last_anim      = 0xFFFFFFFFU;

/* 左侧信息区宽度 */
#define OTA_LEFT_MAX_X  79
/* 右侧动画区起点 */
#define OTA_RIGHT_X     80

/* ===== OTA 静态图标 (48x48, page优先横向排列, 6页x48列=288字节) =====
 * 数据布局: page0的48列, page1的48列, ... page5的48列
 * 与 OLED_DrawBMP 兼容, 直接连续写入即可
 */
static const u8 ota_icon_48x46[288] = {
    /* page0 (y=0~7) */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0xF0,0xF0,
    0x60,0xB0,0xDF,0xEF,0xF7,0xFB,0xFC,0xFE,0xFF,0xFD,0xFF,0xFE,0xDE,0xFF,0xFF,0xFE,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* page1 (y=8~15) */
    0x00,0x00,0x00,0x00,0x00,0xE0,0xE0,0xC0,0x60,0xB6,0xDE,0xEE,0xF6,0xFA,0xFD,0xFE,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,0xBF,0xDF,0xEF,0xF7,0xFB,0xFC,
    0xFE,0xFC,0xFE,0xFE,0xEE,0xDE,0xE6,0x66,0xE0,0x80,0xE0,0xE0,0x00,0x00,0x00,0x00,
    /* page2 (y=16~23) */
    0x00,0xFE,0xFE,0xEC,0xF6,0xFB,0xFD,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x8F,0xFF,
    0xFF,0xFB,0xFB,0xFB,0xF3,0x9D,0xFF,0xFD,0xFE,0xFF,0xFF,0x6F,0xFF,0xFB,0xFB,0xFF,
    0xFF,0x9F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFD,0xFF,0xF7,0xEE,0xFE,0xFE,0xF6,
    /* page3 (y=24~31) */
    0x00,0x03,0x03,0x06,0x07,0x7B,0x7F,0x6F,0x5F,0xBF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFE,0xFE,0xFE,0xFE,0xFE,0xFF,0x7F,0xBF,0xF7,0xEF,0xF7,0xFF,0xFD,0xFD,0xFF,0xFD,
    0xFD,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x6F,0x7F,0x3B,0x7F,0x02,0x03,0x03,0x03,
    /* page4 (y=32~39) */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x07,0x06,0x05,0x3B,0x7F,0x7F,
    0x7F,0x3B,0x75,0xFB,0xFD,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x7F,0x7F,0x7F,0x3F,0x07,0x07,0x07,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* page5 (y=40~47, 实际46行, 底部2行空白) */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x07,0x07,0x05,0x05,0x37,0x3F,0x3F,0x37,0x3B,0x07,0x07,0x07,0x07,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

/* 辅助: 格式化字节为 "XXKB"/"XX.XMB" */
static void OTA_FmtBytes(uint32_t bytes, char *buf, uint8_t buf_size)
{
    if (bytes >= 1024UL * 1024UL)
    {
        uint32_t mb_int = bytes / (1024UL * 1024UL);
        uint32_t rem    = (bytes % (1024UL * 1024UL)) / (102UL * 1024UL);
        snprintf(buf, buf_size, "%lu.%luMB", (unsigned long)mb_int, (unsigned long)rem);
    }
    else if (bytes >= 1024UL)
    {
        snprintf(buf, buf_size, "%luKB", (unsigned long)(bytes / 1024UL));
    }
    else
    {
        snprintf(buf, buf_size, "%luB", (unsigned long)bytes);
    }
}

/* 辅助: 用空格填充指定区域 (局部擦除) */
static void OTA_EraseRange(uint8_t x_start, uint8_t x_end, uint8_t y_page)
{
    uint8_t col_end = (x_end <= OTA_LEFT_MAX_X + 1) ? x_end : (OTA_LEFT_MAX_X + 1);
    if (col_end <= x_start) return;
    OLED_Fill(x_start, y_page, col_end - 1, y_page, 0);
}

/* ============ 普通模式: 初始化显示界面 ============ */
void OLED_Display_Init(void)
{
    OLED_Clear();

    OLED_ShowString(0, 4, (u8 *)"stm32", 16);
    OLED_ShowCHinese(40, 4, IDX_HAN_XIAO);
    OLED_ShowCHinese(56, 4, IDX_HAN_FENG);
    OLED_ShowCHinese(72, 4, IDX_HAN_SHAN);

    s_ota_mode = 0;
    s_ota_stage = OTA_STAGE_IDLE;
}

void OLED_SetBTStatus(conn_status_t status)   { s_bt_status   = status; }
void OLED_SetWiFiStatus(conn_status_t status)  { s_wifi_status = status; }
void OLED_SetGear(uint8_t gear)                 { s_gear        = gear; }

/* 格式化运行时间 */
static void Format_Time(uint32_t sec, char *buf, uint16_t buf_size)
{
    uint32_t hours = (sec % 86400) / 3600;
    uint32_t mins  = (sec % 3600) / 60;
    uint32_t secs  = sec % 60;

    if (sec < 60)
        snprintf(buf, buf_size, "Time: %lus", (unsigned long)secs);
    else if (sec < 3600)
        snprintf(buf, buf_size, "Time: %02lu:%02lu", (unsigned long)mins, (unsigned long)secs);
    else
        snprintf(buf, buf_size, "Time: %02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
}

/* 普通模式刷新 (每秒调用) */
void OLED_Display_Refresh(uint32_t elapsed_sec)
{
    char line_buf[20];
    if (s_ota_mode) return;

    OLED_Clear();

    /* 行0: 蓝牙状态 + bemfa状态 */
    OLED_ShowCHinese(0,  0, IDX_HAN_LAN);
    OLED_ShowCHinese(16, 0, IDX_HAN_YA);
    OLED_ShowString(32, 0, (u8 *)":", 16);

    if (s_bt_status == STATUS_CONNECTED)
        OLED_ShowString(40, 0, (u8 *)"OK", 16);
    else if (s_bt_status == STATUS_ON)
        OLED_ShowCHinese(40, 0, IDX_HAN_KAI);
    else
        OLED_ShowCHinese(40, 0, IDX_HAN_GUAN);

    OLED_ShowString(56, 0, (u8 *)" bemfa:", 16);

    if (s_wifi_status == STATUS_CONNECTED)
        OLED_ShowString(113, 0, (u8 *)"OK", 16);
    else if (s_wifi_status == STATUS_ON)
        OLED_ShowCHinese(113, 0, IDX_HAN_KAI);
    else
        OLED_ShowCHinese(113, 0, IDX_HAN_GUAN);

    /* 行3: 档位 */
    OLED_ShowCHinese(0,  3, IDX_HAN_DANG);
    OLED_ShowCHinese(16, 3, IDX_HAN_WEI);
    snprintf(line_buf, sizeof(line_buf), ":%d", s_gear);
    OLED_ShowString(32, 3, (u8 *)line_buf, 16);

    /* 行6: 时间 */
    OLED_ShowCHinese(0,  6, IDX_HAN_SHI);
    OLED_ShowCHinese(16, 6, IDX_HAN_JIAN);
    OLED_ShowString(32, 6, (u8 *)":", 16);
    Format_Time(elapsed_sec, line_buf, sizeof(line_buf));
    OLED_ShowString(48, 6, (u8 *)(line_buf + 6), 16);
}


/* ===========================================================================
 *                     OTA 升级模式 OLED 显示 (3行布局)
 *
 *   行0 (y=0, page0-1): "OTA升级中"  静态标题
 *   行3 (y=3, page3-4): 阶段动词 (5汉字, 80px)
 *   行6 (y=6, page6):   进度条(64px) + 百分比(16px)
 *
 *   右侧 x=80~127 (48px): 预留动画区
 * =========================================================================== */

/* 行0: 静态标题 "OTA升级中" (OLED_OTA_Init时画一次) */
static void _OTA_DrawTitle(void)
{
    OLED_Clear();
    s_ota_mode = 1;

    /* "OTA" ASCII 16号 (x=0, 3字符=24px) */
    OLED_ShowString(0, 0, (u8 *)"OTA", 16);
    /* "升" (x=28) */
    OLED_ShowCHinese(28, 0, IDX_HAN_SHENG2);
    /* "级" (x=44) */
    OLED_ShowCHinese(44, 0, IDX_HAN_JI3);
    /* "中" (x=60) */
    OLED_ShowCHinese(60, 0, IDX_HAN_ZHONG);

    /* 重置last状态 */
    s_last_stage = (ota_stage_t)0xFF;
    s_last_pct   = 0xFFFFFFFFU;
    s_last_done  = 0xFFFFFFFFU;
    s_last_final = (ota_stage_t)0xFF;
    s_last_err   = 0xFFFFFFFFU;
    s_last_cd    = 0xFFFFFFFFU;
    s_last_anim  = 0xFFFFFFFFU;
}

/* 行3: 阶段动词刷新 (5汉字80px, 防重绘) */
static void _OTA_RefreshStage(void)
{
    if (s_ota_stage == s_last_stage && s_ota_final == s_last_final)
        return;
    s_last_stage = s_ota_stage;
    s_last_final = s_ota_final;

    /* 擦行3全区域 (page3-4, x=0~78) */
    OLED_Fill(0, 3, 78, 4, 0);

    switch (s_ota_stage)
    {
    case OTA_STAGE_NET:
        /* "网络连接中" 5字80px */
        OLED_ShowCHinese(0,  3, IDX_HAN_WANG);   /* 网 */
        OLED_ShowCHinese(16, 3, IDX_HAN_LUO);    /* 络 */
        OLED_ShowCHinese(32, 3, IDX_HAN_LIAN);   /* 连 */
        OLED_ShowCHinese(48, 3, IDX_HAN_JIE);    /* 接 */
        OLED_ShowCHinese(64, 3, IDX_HAN_ZHONG);  /* 中 */
        break;
    case OTA_STAGE_DL:
        /* "获取升级中" 5字80px */
        OLED_ShowCHinese(0,  3, IDX_HAN_HUO);    /* 获 */
        OLED_ShowCHinese(16, 3, IDX_HAN_QU);     /* 取 */
        OLED_ShowCHinese(32, 3, IDX_HAN_SHENG2);  /* 升 */
        OLED_ShowCHinese(48, 3, IDX_HAN_JI3);    /* 级 */
        OLED_ShowCHinese(64, 3, IDX_HAN_BAO);  /* 包 */
        break;
    case OTA_STAGE_BURN:
        /* "升级烧录中" 5字80px */
        OLED_ShowCHinese(0,  3, IDX_HAN_SHENG2);  /* 升 */
        OLED_ShowCHinese(16, 3, IDX_HAN_JI3);    /* 级 */
        OLED_ShowCHinese(32, 3, IDX_HAN_SHAO);   /* 烧 */
        OLED_ShowCHinese(48, 3, IDX_HAN_LU4);    /* 录 */
        OLED_ShowCHinese(64, 3, IDX_HAN_ZHONG);  /* 中 */
        break;
    case OTA_STAGE_OK:
        /* "跳转中" 3字48px */
        OLED_ShowCHinese(0,  3, IDX_HAN_TIAO);   /* 跳 */
        OLED_ShowCHinese(16, 3, IDX_HAN_ZHUAN);  /* 转 */
        OLED_ShowCHinese(32, 3, IDX_HAN_ZHONG);  /* 中 */
        break;
    case OTA_STAGE_ERR:
        /* "错误" 2字32px */
        OLED_ShowCHinese(0,  3, IDX_HAN_CUO);    /* 错 */
        OLED_ShowCHinese(16, 3, IDX_HAN_WU4);    /* 误 */
        break;
    default:
        break;
    }
}

/* 行6: 进度条 + 百分比 (y=6, page6, 8像素高)
 * 进度条: x=0~63 (64像素宽, 实心矩形)
 * 百分比: x=64~79 (8号字体, "62%")
 */
static void _OTA_DrawProgress(uint32_t done, uint32_t total)
{
    uint32_t pct;
    char pct_buf[8];
    uint8_t bar_x_end;

    /* 结果状态时: 不刷新进度条 (保持最终状态) */
    if (s_ota_final == OTA_STAGE_OK || s_ota_final == OTA_STAGE_ERR)
        return;

    if (total == 0)
    {
        /* 无总长度: 显示已接收字节数 */
        if (done == s_last_done) return;
        s_last_done = done;

        OLED_Fill(0, 6, 78, 6, 0);
        {
            char done_buf[16];
            OTA_FmtBytes(done, done_buf, sizeof(done_buf));
            OLED_ShowString(0, 6, (u8 *)done_buf, 8);
            OLED_ShowString(strlen(done_buf) * 6 + 2, 6, (u8 *)"...", 8);
        }
        s_last_pct = 0xFFFFFFFFU;
        return;
    }

    pct = (done >= total) ? 100 : (uint32_t)((uint64_t)done * 100ULL / (uint64_t)total);

    if (pct != s_last_pct)
    {
        s_last_pct = pct;

        /* 进度条: 左侧0~78全宽 (79px) */
        OLED_Fill(0, 6, 78, 6, 0);
        bar_x_end = (uint8_t)(pct * 79UL / 100UL);
        if (bar_x_end > 0)
            OLED_Fill(0, 6, bar_x_end - 1, 6, 1);

        /* 百分比文字: 右侧动画区下方 (page6, x=82居中) */
        OLED_Fill(80, 6, 127, 6, 0);
        snprintf(pct_buf, sizeof(pct_buf), "%lu%%", (unsigned long)pct);
        OLED_ShowString(82, 6, (u8 *)pct_buf, 8);
    }

    /* 更新done缓存 */
    s_last_done = done;
}

/* 行6底部: 结果提示 (错误码/倒计时)
 * 成功: "OK 3s" (倒计时)
 * 失败: "E:07"  (错误码)
 */
static void _OTA_RefreshResult(void)
{
    if (s_ota_final != OTA_STAGE_OK && s_ota_final != OTA_STAGE_ERR)
        return;

    if (s_ota_final == s_last_final &&
        s_ota_err == s_last_err &&
        s_ota_cd == s_last_cd)
        return;

    s_last_final = s_ota_final;
    s_last_err = s_ota_err;
    s_last_cd = s_ota_cd;

    /* 清行6底部 */
    OLED_Fill(0, 6, 78, 6, 0);

    if (s_ota_final == OTA_STAGE_OK)
    {
        /* 成功: "OK Xs" */
        char line[16];
        snprintf(line, sizeof(line), "OK %lus", (unsigned long)s_ota_cd);
        OLED_ShowString(0, 6, (u8 *)line, 8);
    }
    else
    {
        /* 失败: "E:XX" */
        char line[16];
        snprintf(line, sizeof(line), "E:%02lu", (unsigned long)s_ota_err);
        OLED_ShowString(0, 6, (u8 *)line, 8);
    }
}

/* ============ OTA公开API ============ */

void OLED_OTA_Init(ota_part_t target_part)
{
    s_ota_part  = target_part;
    s_ota_stage = OTA_STAGE_NET;
    s_ota_final = OTA_STAGE_IDLE;
    s_ota_err   = 0;
    s_ota_cd    = 0;

    /* 行0: 标题 (含清屏) */
    _OTA_DrawTitle();

    /* 行3: 初始阶段 "网络连接中" */
    _OTA_RefreshStage();

    /* 行6: 清空进度条整行(含右侧百分比区) */
    OLED_Fill(0, 6, 127, 6, 0);

    /* 右侧动画区: 绘制静态OTA图标 (48x48, x=80~127, y=0~5) */
    OLED_OTA_DrawIcon();
}

void OLED_OTA_SetStage(ota_stage_t stage)
{
    if (s_ota_stage != stage)
    {
        /* 阶段切换: 重置进度条缓存, 清空进度行 */
        s_ota_stage = stage;
        s_last_pct  = 0xFFFFFFFFU;
        s_last_done = 0xFFFFFFFFU;
        OLED_Fill(0, 6, 127, 6, 0);
    }
    _OTA_RefreshStage();
}

void OLED_OTA_SetProgress(uint32_t done, uint32_t total)
{
    _OTA_DrawProgress(done, total);
}

void OLED_OTA_SetResult(ota_stage_t final_stage, uint32_t err_code, uint32_t countdown_sec)
{
    s_ota_stage = final_stage;
    s_ota_final = final_stage;
    s_ota_err   = err_code;
    s_ota_cd    = countdown_sec;

    /* 行3: 显示 "跳转中" 或 "错误" */
    _OTA_RefreshStage();
    /* 行6: 显示结果 */
    _OTA_RefreshResult();
}

/* 绘制右侧静态图标 (48x48, 占x=80~127, y=0~5共6页)
 * 字模为page优先横向排列, 直接按页连续写入
 */
void OLED_OTA_DrawIcon(void)
{
    uint8_t page, col;
    for (page = 0; page < 6; page++)
    {
        OLED_Set_Pos(OTA_RIGHT_X, page);
        for (col = 0; col < 48; col++)
        {
            OLED_WR_Byte(ota_icon_48x46[page * 48 + col], OLED_DATA);
        }
    }
}

/* 右侧动画区占位 (后期添加图标动画) */
void OLED_OTA_AnimationTick(uint32_t frame_idx)
{
    if (!s_ota_mode) return;
    if (frame_idx == s_last_anim) return;
    s_last_anim = frame_idx;

    /* 简单呼吸点动画: 3个点循环 */
    OLED_Fill(OTA_RIGHT_X + 8, 3, OTA_RIGHT_X + 40, 4, 0);
    switch (frame_idx % 4)
    {
    case 0: OLED_ShowString(OTA_RIGHT_X + 16, 3, (u8 *)".  ", 16); break;
    case 1: OLED_ShowString(OTA_RIGHT_X + 16, 3, (u8 *)".. ", 16); break;
    case 2: OLED_ShowString(OTA_RIGHT_X + 16, 3, (u8 *)"...", 16); break;
    case 3: OLED_ShowString(OTA_RIGHT_X + 16, 3, (u8 *)"   ", 16); break;
    }
}

/* 显示"已经是最新版本"提示 (版本对比失败时调用)
 * 清屏 + 行3居中显示7汉字
 */
void OLED_OTA_ShowLatest(void)
{
    /* 清屏(含动画区) */
    OLED_Clear();
    s_ota_mode = 0;

    /* "已经是最新版本" 7字 = 112px, 居中起始 x=(128-112)/2=8 */
    OLED_ShowCHinese(8,   3, IDX_HAN_YI2);    /* 已 */
    OLED_ShowCHinese(24,  3, IDX_HAN_JING);   /* 经 */
    OLED_ShowCHinese(40,  3, IDX_HAN_SHI3);   /* 是 */
    OLED_ShowCHinese(56,  3, IDX_HAN_ZUI);    /* 最 */
    OLED_ShowCHinese(72,  3, IDX_HAN_XIN);    /* 新 */
    OLED_ShowCHinese(88,  3, IDX_HAN_BAN);    /* 版 */
    OLED_ShowCHinese(104, 3, IDX_HAN_BEN);    /* 本 */
}
