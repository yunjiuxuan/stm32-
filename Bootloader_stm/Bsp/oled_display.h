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

//初始化显示界面 (Bootloader启动logo + 普通跳转模式)
void OLED_Display_Init(void);

//设置蓝牙连接状态
void OLED_SetBTStatus(conn_status_t status);

//设置WiFi连接状态
void OLED_SetWiFiStatus(conn_status_t status);

//设置风扇档位（0=停止，1~n=对应档位）
void OLED_SetGear(uint8_t gear);

//刷新OLED显示（每秒调用一次） - 普通运行模式 (非OTA)
//参数: elapsed_sec - 从启动开始的秒数
void OLED_Display_Refresh(uint32_t elapsed_sec);

/* ================================================================
 *               OTA 升级模式显示 (Bootloader专用)
 *   3行精简布局, 左侧80px信息区, 右侧x=80~127留给动画图标
 *
 *   行0 (y=0): "OTA升级中"  静态标题
 *   行3 (y=3): 阶段动词 (5汉字80px)
 *              网络连接中 / 获取升级中 / 升级烧录中 / 跳转中 / 错误
 *   行6 (y=6): 进度条(64px) + 百分比(16px)
 * ================================================================ */

/* OTA阶段枚举 - 对应行3的动词显示 */
typedef enum {
    OTA_STAGE_IDLE = 0,  /* 未进入OTA */
    OTA_STAGE_NET  = 1,  /* 网络连接中: ESP8266初始化+版本查询+JSON解析 */
    OTA_STAGE_DL   = 2,  /* 获取升级中: HTTP下载bin -> W25Q128 */
    OTA_STAGE_BURN = 3,  /* 升级烧录中: 擦除+编程+校验内部Flash */
    OTA_STAGE_OK   = 4,  /* 跳转中: 成功, 倒计时后复位 */
    OTA_STAGE_ERR  = 5   /* 错误: 失败, 显示错误码 */
} ota_stage_t;

/* 目标分区 */
typedef enum {
    OTA_PART_A = 0,   /* -> A区 */
    OTA_PART_B = 1    /* -> B区 */
} ota_part_t;

/* 错误码 */
#define OTA_ERR_NONE              0
#define OTA_ERR_ESP8266_INIT      1
#define OTA_ERR_WIFI_CONNECT      2
#define OTA_ERR_META_HTTP         3
#define OTA_ERR_META_PARSE        4
#define OTA_ERR_BIN_NO_NEW        5
#define OTA_ERR_BIN_HTTP          6
#define OTA_ERR_BIN_DL_SHORT      7
#define OTA_ERR_W25Q_ERASE        8
#define OTA_ERR_FLASH_ERASE       9
#define OTA_ERR_FLASH_PROG        10
#define OTA_ERR_VERIFY            11
#define OTA_ERR_IPD_OVERFLOW      12

/* =============== OTA公开API (3行布局) =============== */

/* 进入OTA模式后调用一次: 清屏+绘制标题"OTA升级中"
 * target_part: OTA_PART_A / OTA_PART_B
 */
void OLED_OTA_Init(ota_part_t target_part);

/* 更新阶段 (行3显示5汉字动词)
 * OTA_STAGE_NET  -> "网络连接中"
 * OTA_STAGE_DL   -> "获取升级中"
 * OTA_STAGE_BURN -> "升级烧录中"
 * OTA_STAGE_OK   -> "跳转中"
 * OTA_STAGE_ERR  -> "错误"
 */
void OLED_OTA_SetStage(ota_stage_t stage);

/* 更新进度 (行6: 进度条+百分比)
 * done:  已完成字节数
 * total: 总字节数 (传0表示未知,仅显示done)
 */
void OLED_OTA_SetProgress(uint32_t done, uint32_t total);

/* 设置结束状态 (行3动词+行6底部提示)
 * final_stage: OTA_STAGE_OK / OTA_STAGE_ERR
 * err_code:    错误码 (成功时忽略)
 * countdown_sec: 成功时倒计时秒数
 */
void OLED_OTA_SetResult(ota_stage_t final_stage, uint32_t err_code, uint32_t countdown_sec);

/* 右侧动画区占位 (后期添加图标动画)
 * frame_idx: 帧索引, 外部每200ms自增一次
 */
void OLED_OTA_AnimationTick(uint32_t frame_idx);

/* ===== 汉字索引 (与oledfont.h Hzk数组一致) ===== */
#define IDX_HAN_LAN    14  // 蓝
#define IDX_HAN_YA     15  // 牙
#define IDX_HAN_KAI    16  // 开
#define IDX_HAN_GUAN   17  // 关
#define IDX_HAN_SHI    18  // 时
#define IDX_HAN_JIAN   19  // 间
#define IDX_HAN_DANG   20  // 档
#define IDX_HAN_WEI    21  // 位
#define IDX_HAN_XIAO   22  // 小
#define IDX_HAN_FENG   23  // 风
#define IDX_HAN_SHAN   24  // 扇
#define IDX_HAN_GOU    25  // √
#define IDX_HAN_YOU    26  // 有
#define IDX_HAN_REN    27  // 人
#define IDX_HAN_WU     28  // 无
#define IDX_HAN_DEGREE 29  // °
#define IDX_HAN_GANG   30  // 刚(杠)

/* OTA汉字 (索引31~49) */
#define IDX_HAN_SHENG  31  // 升
#define IDX_HAN_JI2    32  // 级
#define IDX_HAN_JIAN3  33  // 检
#define IDX_HAN_CHA    34  // 查
#define IDX_HAN_XIA4   35  // 下
#define IDX_HAN_ZAI    36  // 载
#define IDX_HAN_SHAO   37  // 烧
#define IDX_HAN_LU4    38  // 录
#define IDX_HAN_CA     39  // 擦
#define IDX_HAN_CHU2   40  // 除
#define IDX_HAN_JIAO   41  // 校
#define IDX_HAN_YAN4   42  // 验
#define IDX_HAN_CUO    43  // 错
#define IDX_HAN_WU4    44  // 误
#define IDX_HAN_TIAO   45  // 跳
#define IDX_HAN_ZHUAN  46  // 转
#define IDX_HAN_DAO     47  // 倒
#define IDX_HAN_JI4    48  // 计
#define IDX_HAN_SHI2   49  // 时(备用)

/* 3行布局新增汉字 (索引50~59, 10x10宋体转16x16居中) */
#define IDX_HAN_WANG   50  // 网
#define IDX_HAN_LUO    51  // 络
#define IDX_HAN_LIAN   52  // 连
#define IDX_HAN_JIE    53  // 接
#define IDX_HAN_HUO    54  // 获
#define IDX_HAN_QU     55  // 取
#define IDX_HAN_BAO    56  // 包
#define IDX_HAN_ZHONG  57  // 中
#define IDX_HAN_SHENG2 58  // 升 (10x10宋体, 用于3行布局)
#define IDX_HAN_JI3    59  // 级 (10x10宋体, 用于3行布局)

/* 版本提示汉字 (索引60~66, 10x10宋体转16x16) */
#define IDX_HAN_YI2    60  // 已
#define IDX_HAN_JING   61  // 经
#define IDX_HAN_SHI3   62  // 是
#define IDX_HAN_ZUI     63  // 最
#define IDX_HAN_XIN     64  // 新
#define IDX_HAN_BAN     65  // 版
#define IDX_HAN_BEN     66  // 本

/* 显示"已经是最新版本"提示 (版本对比失败时调用) */
void OLED_OTA_ShowLatest(void);

/* 绘制右侧静态OTA图标 (48x48, x=80~127, y=0~5) */
void OLED_OTA_DrawIcon(void);

#endif //__OLED_DISPLAY_H
