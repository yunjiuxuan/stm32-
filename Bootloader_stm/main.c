#include "stm32f4xx.h"
#include "reg_led.h"
#include "func_led.h"
#include "version.h"
#include "dsp_usart.h"
#include "delay.h"
#include "oled.h"
#include "oled_display.h"

extern uint8_t  _logflg;
extern uint8_t  cmd[];
extern uint8_t  _bleflg;
extern uint8_t  BLE_String[];

void Boot_USART1_Init(void);
uint8_t Check_APP_Valid(uint32_t app_addr);
void Jump_To_Application(uint32_t app_addr);
void Record_LastBoot(uint32_t app_addr);
void Boot_Partition(uint32_t app_addr, uint16_t led_pin, GPIO_TypeDef* led_port);

const version_info_t g_version = {
    VERSION_MAGIC, 1, 0, 20260814, "Bootloader"
};

#define APP_A_ADDRESS  0x08040000
#define APP_B_ADDRESS  0x080A0000

/* APP长按按键写入BKP0R, 请求Bootloader执行OTA并写到目标分区 */
#define OTA_UPGRADE_A    0xA5A50003u   /* 当前APP运行B,请求升级到A后跳A */
#define OTA_UPGRADE_B    0xA5A50004u   /* 当前APP运行A,请求升级到B后跳B */

/* Bootloader完成OTA后写入BKP0R,请求下次启动直接跳目标APP */
#define JUMP_FLAG_TO_A   0xA5A50001u
#define JUMP_FLAG_TO_B   0xA5A50002u
/* Bootloader跳转后写入BKP1R,记录上次运行哪个分区 */
#define LAST_BOOT_TO_A   0x5A5A0001u
#define LAST_BOOT_TO_B   0x5A5A0002u

void Boot_USART1_Init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);

    USART_InitTypeDef uart;
    USART_StructInit(&uart);
    uart.USART_BaudRate = 115200;
    USART_Init(USART1, &uart);
    USART_Cmd(USART1, ENABLE);
}

 

 
void Boot_PrintU(uint32_t n)
{
    char buf[12];
    int i = 11;
    buf[11] = 0;
    if (n == 0) { Boot_Print("0"); return; }
    while (n) { buf[--i] = (char)('0' + (n % 10)); n /= 10; }
    Boot_Print(&buf[i]);
}

void Boot_PrintVersion(void)
{
    Boot_Print("\r\n=== ");
    Boot_Print(g_version.name);
    Boot_Print(" V");
    Boot_PrintU(g_version.ver_major);
    Boot_Print(".");
    Boot_PrintU(g_version.ver_minor);
    Boot_Print(" build ");
    Boot_PrintU(g_version.build_date);
    Boot_Print(" ===\r\n");
}

uint8_t Check_APP_Valid(uint32_t app_addr)
{
    volatile uint32_t *pAddr = (volatile uint32_t *)app_addr;
    uint32_t sp = pAddr[0];
    uint32_t reset_handler = pAddr[1];

    int sp_valid = 0;
    if ((sp >= 0x20000000) && (sp < 0x20020000))      sp_valid = 1;
    else if ((sp >= 0x10000000) && (sp < 0x10010000)) sp_valid = 1;

    if (sp_valid) {
        if ((reset_handler >= 0x08040000) && (reset_handler < 0x08100000)) {
            if (reset_handler & 1) return 1;
        }
    }
    return 0;
}

typedef void (*pFunction)(void);
void Jump_To_Application(uint32_t app_addr)
{
    volatile uint32_t *pAddr = (volatile uint32_t *)app_addr;

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    SCB->VTOR = app_addr;
    __set_MSP(pAddr[0]);
    __enable_irq();
    pFunction JumpToApp = (pFunction)(pAddr[1]);
    JumpToApp();
}

void Record_LastBoot(uint32_t app_addr)
{
    PWR_BackupAccessCmd(ENABLE);
    RTC->BKP1R = (app_addr == APP_A_ADDRESS) ? LAST_BOOT_TO_A : LAST_BOOT_TO_B;
}

void Boot_Partition(uint32_t app_addr, uint16_t led_pin, GPIO_TypeDef* led_port)
{
    Record_LastBoot(app_addr);

    GPIO_ResetBits(led_port, led_pin); delay_ms(300);
    GPIO_SetBits(led_port, led_pin);   delay_ms(300);
    GPIO_ResetBits(led_port, led_pin); delay_ms(300);
    GPIO_SetBits(led_port, led_pin);   delay_ms(300);

    Jump_To_Application(app_addr);
}

int main(void)
{
    Delay_Init();
    Func_LED_Init();
    PA2_3_USART2_Init(9600);
    PA9_10_USART1_Init(115200);

    GPIO_ResetBits(GPIOF, GPIO_Pin_9); delay_ms(200);
    GPIO_SetBits(GPIOF, GPIO_Pin_9);   delay_ms(200);
    GPIO_ResetBits(GPIOF, GPIO_Pin_9); delay_ms(200);
    GPIO_SetBits(GPIOF, GPIO_Pin_9);   delay_ms(200);
    delay_ms(100);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    /* ===== 第一步: 先读BKP0R, 判断是否需要执行OTA ===== */
    uint32_t jump_req = RTC->BKP0R;

    if (jump_req == OTA_UPGRADE_A || jump_req == OTA_UPGRADE_B)
    {
        /* APP长按按键请求OTA升级: 才初始化ESP8266并联网下载
           注意: 此时不清零BKP0R, 让OTA_FlashAndJump()能读到OTA_UPGRADE_A/B
           来判断写入目标分区; OTA_FlashAndJump内部会重写BKP0R为JUMP_FLAG */
        BLE_PRINTF("[BOOT] 检测到OTA请求标志 BKP0R=0x%08X, 启动升级流程\r\n", jump_req);

        /* 初始化OLED并显示OTA升级界面 */
        OLED_Init();
        {
            ota_part_t target = (jump_req == OTA_UPGRADE_A) ? OTA_PART_A : OTA_PART_B;
            OLED_OTA_Init(target);
        }

        BLE_PRINTF("[BOOT] 初始化 USART3(ESP8266) @115200\r\n");
        PB10_11_ESP8266_Init(115200);
        BLE_PRINTF("[BOOT] 开始 OTA_HTTP_Test()\r\n");
        OTA_HTTP_Test();
        /* OTA_HTTP_Test() 内部的 OTA_FlashAndJump() 会设置 JUMP_FLAG_TO_A/B
           并触发系统复位; 若OTA失败返回到这里, 清除OTA标志后继续正常启动 */
        BLE_PRINTF("[BOOT] OTA_HTTP_Test() 返回(升级失败或跳过), 清除OTA标志进入正常启动\r\n");
        RTC->BKP0R = 0;
        jump_req = 0;
    }
    else
    {
        /* 非OTA请求: 清除BKP0R后正常启动 */
        RTC->BKP0R = 0;
        BLE_PRINTF("[BOOT] 无OTA请求标志 BKP0R=0x%08X, 直接启动APP\r\n", jump_req);
    }

    /* ===== 第二步: 正常启动流程 (直接跳转APP) ===== */
    /* 优先级1: OTA完成后写入的 JUMP_FLAG_TO_A/B */
    if (jump_req == JUMP_FLAG_TO_A && Check_APP_Valid(APP_A_ADDRESS))
        Boot_Partition(APP_A_ADDRESS, GPIO_Pin_10, GPIOF);
    else if (jump_req == JUMP_FLAG_TO_B && Check_APP_Valid(APP_B_ADDRESS))
        Boot_Partition(APP_B_ADDRESS, GPIO_Pin_13, GPIOE);

    /* 优先级2: 上次运行记录 BKP1R */
    {
        uint32_t last = RTC->BKP1R;
        if (last == LAST_BOOT_TO_A && Check_APP_Valid(APP_A_ADDRESS))
            Boot_Partition(APP_A_ADDRESS, GPIO_Pin_10, GPIOF);
        else if (last == LAST_BOOT_TO_B && Check_APP_Valid(APP_B_ADDRESS))
            Boot_Partition(APP_B_ADDRESS, GPIO_Pin_13, GPIOE);
    }

    /* 优先级3: 默认A, 无则B */
    if (Check_APP_Valid(APP_A_ADDRESS))
        Boot_Partition(APP_A_ADDRESS, GPIO_Pin_10, GPIOF);
    else if (Check_APP_Valid(APP_B_ADDRESS))
        Boot_Partition(APP_B_ADDRESS, GPIO_Pin_13, GPIOE);

    /* ===== 第三步: AB分区均无效 -> 救砖OTA,强制下载到A区 ===== */
    /* 明确写入 BKP0R=OTA_UPGRADE_A,让 OTA_FlashAndJump 确定目标=A,与OLED显示一致 */
    RTC->BKP0R = OTA_UPGRADE_A;
    BLE_PRINTF("[BOOT] AB分区均无效,启动救砖OTA强制下载到A区\r\n");
    OLED_Init();
    OLED_OTA_Init(OTA_PART_A);
    PB10_11_ESP8266_Init(115200);
    OTA_HTTP_Test();
    /* 成功不会返回(内部已复位);失败延时2秒后复位重试,避免卡死 */
    BLE_PRINTF("[BOOT] 救砖OTA失败,2秒后复位重试\r\n");
    RTC->BKP0R = 0;
    delay_s(2);
    NVIC_SystemReset();

    /* 理论上不可达,保留以防万一 */
    while (1) {
        GPIO_ResetBits(GPIOE, GPIO_Pin_14); delay_ms(300);
        GPIO_SetBits(GPIOE, GPIO_Pin_14);   delay_ms(300);
    }
}
