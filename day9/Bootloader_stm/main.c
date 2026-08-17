#include "stm32f4xx.h"

/* Custom headers */
#include "reg_led.h"
#include "func_led.h"
#include "version.h"
#include "dsp_usart.h"
#include "delay.h"
#include "oled.h"
#include "oled_display.h"

/* 引用 dsp_usart.c 中定义的变量和函数声明 */
extern uint8_t  _logflg;
extern uint8_t  cmd[];
extern uint8_t  _bleflg;
extern uint8_t  BLE_String[];
extern int OTA_Download_And_Flash(uint32_t target_addr);

/* ===== Forward declarations for Bootloader helper functions ===== */
void Boot_USART1_Init(void);
void Boot_Print(const char *s);
void Boot_PrintU(uint32_t n);
void Boot_PrintVersion(void);
uint8_t Check_APP_Valid(uint32_t app_addr);
void Jump_To_Application(uint32_t app_addr);
void Record_LastBoot(uint32_t app_addr);
void Boot_Partition(uint32_t app_addr, uint16_t led_pin, GPIO_TypeDef* led_port);

/*
Use GPIO: enable RCC clock, GPIO on AHB1.
PF9 = LED0 (Bootloader status indicator)
*/

/* ===== Version info (stored in Flash, survives power-down) ===== */
const version_info_t g_version = {
    VERSION_MAGIC, 1, 0, 20260814, "Bootloader"
};

/* ===== Flash partition map (three-way split, A=B=384KB) ===== */
/* Bootloader: sector 0-5, 0x08000000 - 0x0803FFFF (256KB) */
/* A: sector 6-8, 0x08040000 - 0x0809FFFF (384KB) */
/* B: sector 9-11, 0x080A0000 - 0x080FFFFF (384KB) */
#define APP_A_ADDRESS  0x08040000
#define APP_B_ADDRESS  0x080A0000

/* ===== RTC backup register flags ===== */
/* BKP0R: APP partition-switch request (written by APP before NVIC_SystemReset) */
#define JUMP_FLAG_TO_A   0xA5A50001u
#define JUMP_FLAG_TO_B   0xA5A50002u
#define OTA_UPGRADE_A    0xA5A50003u
#define OTA_UPGRADE_B    0xA5A50004u
/* BKP1R: last-boot record (written by Bootloader before each jump, sticky) */
#define LAST_BOOT_TO_A   0x5A5A0001u
#define LAST_BOOT_TO_B   0x5A5A0002u

 
/* ===== Minimal USART1 (PA9, TX-only) for boot log ===== */
/* 简化版 USART1 初始化, 不使用 PA9_10_USART1_Init() 以避免开启 RX 中断干扰 */
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

/* ===== 通过蓝牙或 USART1 接收 'x' 命令触发 OTA HTTP 测试 ===== */
/* 返回值 1 = 执行了 OTA 测试, 0 = 未收到命令或超时 */
static uint8_t Process_RxCMD(void)
{
    if (_logflg)
    {
        _logflg = 0;
        if (cmd[0] == 'x' || cmd[0] == 'X')
        {
            PA2_3_USART2_Init(115200);
            PB10_11_ESP8266_Init(115200);
            BLE_PRINTF("[BOOT] 收到命令 'x' -> 初始化 BLE(USART2)+ESP8266(USART3) 并启动 OTA_HTTP_Test...\r\n");
            OTA_HTTP_Test();
            BLE_PRINTF("[BOOT] OTA_HTTP_Test 执行完毕.\r\n");
            return 1;
        }
        else if (cmd[0] != '\0')
        {
            BLE_PRINTF("[BOOT] 未知命令 '%s' (发送 'x' + 回车 进行 OTA HTTP 测试)\r\n", (const char *)cmd);
        }
    }
    if (_bleflg)
    {
        _bleflg = 0;
        if (BLE_String[0] == 'x' || BLE_String[0] == 'X')
        {
            PB10_11_ESP8266_Init(115200);
            BLE_PRINTF("[BOOT] 蓝牙收到命令 'x' -> 启动 OTA_HTTP_Test...\r\n");
            OTA_HTTP_Test();
            BLE_PRINTF("[BOOT] OTA_HTTP_Test 执行完毕.\r\n");
            return 1;
        }
        else if (BLE_String[0] != '\0')
        {
            BLE_PRINTF("[BOOT] 未知命令 '%s' (发送 'x' + 回车 进行 OTA HTTP 测试)\r\n", (const char *)BLE_String);
        }
    }
    return 0;
}

/* 等待命令输入窗口, 超时后返回 */
static void WaitCMD_Window(uint32_t ms)
{
    uint32_t loops = ms / 20;
    for (uint32_t i = 0; i < loops; i++)
    {
        Process_RxCMD();
        delay_ms(20);
    }
}

/* Print a string */
void Boot_Print(const char *s)
{
    while (*s) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, (uint16_t)*s++);
    }
}

/* Print an unsigned decimal number */
void Boot_PrintU(uint32_t n)
{
    char buf[12];
    int i = 11;
    buf[11] = 0;
    if (n == 0) { Boot_Print("0"); return; }
    while (n) { buf[--i] = (char)('0' + (n % 10)); n /= 10; }
    Boot_Print(&buf[i]);
}

/* Print version info */
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

/* ===== Check if APP at given address looks valid ===== */
uint8_t Check_APP_Valid(uint32_t app_addr)
{
    volatile uint32_t *pAddr = (volatile uint32_t *)app_addr;
    uint32_t sp = pAddr[0];            /* stack top */
    uint32_t reset_handler = pAddr[1]; /* Reset_Handler address */

    int sp_valid = 0;
    /* SRAM1: 0x20000000 - 0x2001FFFF (128KB) */
    if ((sp >= 0x20000000) && (sp < 0x20020000))      sp_valid = 1;
    /* SRAM2: 0x10000000 - 0x1000FFFF (64KB) */
    else if ((sp >= 0x10000000) && (sp < 0x10010000)) sp_valid = 1;

    if (sp_valid) {
        /* Reset_Handler must be in A/B Flash range */
        if ((reset_handler >= 0x08040000) && (reset_handler < 0x08100000)) {
            if (reset_handler & 1) return 1;  /* Thumb bit */
        }
    }
    return 0;
}

/* ===== Jump to APP at given address ===== */
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

/* ===== Record last-boot partition to BKP1R (sticky across reset) ===== */
void Record_LastBoot(uint32_t app_addr)
{
    PWR_BackupAccessCmd(ENABLE);
    RTC->BKP1R = (app_addr == APP_A_ADDRESS) ? LAST_BOOT_TO_A : LAST_BOOT_TO_B;
}

/* ===== Boot a partition: log + record + LED blink + jump ===== */
void Boot_Partition(uint32_t app_addr, uint16_t led_pin, GPIO_TypeDef* led_port)
{
    if (app_addr == APP_A_ADDRESS) BLE_PRINTF("[BOOT] 跳转到 -> A 分区 (0x08040000)\r\n");
    else                            BLE_PRINTF("[BOOT] 跳转到 -> B 分区 (0x080A0000)\r\n");

    Record_LastBoot(app_addr);

    /* LED blink to indicate target partition */
    GPIO_ResetBits(led_port, led_pin); delay_ms(300);
    GPIO_SetBits(led_port, led_pin);   delay_ms(300);
    GPIO_ResetBits(led_port, led_pin); delay_ms(300);
    GPIO_SetBits(led_port, led_pin);   delay_ms(300);

    Jump_To_Application(app_addr);
}

int main(void)
{
    Func_LED_Init();
    PA9_10_USART1_Init(115200);
    PA2_3_USART2_Init(9600);

    /* LED0 double-blink: Bootloader is running */
    GPIO_ResetBits(GPIOF, GPIO_Pin_9); delay_ms(200);
    GPIO_SetBits(GPIOF, GPIO_Pin_9);   delay_ms(200);
    GPIO_ResetBits(GPIOF, GPIO_Pin_9); delay_ms(200);
    GPIO_SetBits(GPIOF, GPIO_Pin_9);   delay_ms(200);
    delay_ms(100);

    /* Print version */
    BLE_PRINTF("\r\n=== %s V%d.%d build %d ===\r\n",
               g_version.name, g_version.ver_major, g_version.ver_minor, g_version.build_date);
    BLE_PRINTF("[BOOT] 通过蓝牙发送 'x' + 回车 可启动 OTA HTTP 测试 (3秒窗口)...\r\n");
    /* 等待 3 秒命令输入窗口, 收到 'x' 则启动 OTA 测试, 否则继续启动 APP */
    WaitCMD_Window(3000);

    /* Enable backup-domain access */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    uint32_t boot_req = RTC->BKP0R;
    RTC->BKP0R = 0;

    /* (0) OTA upgrade request - highest priority */
    if (boot_req == OTA_UPGRADE_A || boot_req == OTA_UPGRADE_B)
    {
        uint32_t target_addr   = (boot_req == OTA_UPGRADE_A) ? APP_A_ADDRESS : APP_B_ADDRESS;
        uint32_t fallback_addr = (boot_req == OTA_UPGRADE_A) ? APP_B_ADDRESS : APP_A_ADDRESS;

        PA2_3_USART2_Init(115200);
        PB10_11_ESP8266_Init(115200);

        OLED_Init();
        OLED_Clear();

        BLE_PRINTF("[BOOT] OTA upgrade request, target: %s\r\n", (boot_req == OTA_UPGRADE_A) ? "A" : "B");

        int ota_ret = OTA_Download_And_Flash(target_addr);

        if (ota_ret == 0 && Check_APP_Valid(target_addr))
        {
            BLE_PRINTF("[BOOT] OTA OK, jump to new partition\r\n");
            if (target_addr == APP_A_ADDRESS)
                Boot_Partition(target_addr, GPIO_Pin_10, GPIOF);
            else
                Boot_Partition(target_addr, GPIO_Pin_13, GPIOE);
        }
        else
        {
            BLE_PRINTF("[BOOT] OTA FAIL, fallback to old partition\r\n");
            if (Check_APP_Valid(fallback_addr))
            {
                if (fallback_addr == APP_A_ADDRESS)
                    Boot_Partition(fallback_addr, GPIO_Pin_10, GPIOF);
                else
                    Boot_Partition(fallback_addr, GPIO_Pin_13, GPIOE);
            }
        }
    }
    /* (1) APP partition-switch request (BKP0R) */
    if (boot_req == JUMP_FLAG_TO_A && Check_APP_Valid(APP_A_ADDRESS))
        Boot_Partition(APP_A_ADDRESS, GPIO_Pin_10, GPIOF);
    else if (boot_req == JUMP_FLAG_TO_B && Check_APP_Valid(APP_B_ADDRESS))
        Boot_Partition(APP_B_ADDRESS, GPIO_Pin_13, GPIOE);

    /* (2) Last-boot record (BKP1R) - sticky preference */
    uint32_t last = RTC->BKP1R;
    if (last == LAST_BOOT_TO_A && Check_APP_Valid(APP_A_ADDRESS))
        Boot_Partition(APP_A_ADDRESS, GPIO_Pin_10, GPIOF);
    else if (last == LAST_BOOT_TO_B && Check_APP_Valid(APP_B_ADDRESS))
        Boot_Partition(APP_B_ADDRESS, GPIO_Pin_13, GPIOE);

    /* (3) Fallback: A first, then B */
    if (Check_APP_Valid(APP_A_ADDRESS))
        Boot_Partition(APP_A_ADDRESS, GPIO_Pin_10, GPIOF);
    else if (Check_APP_Valid(APP_B_ADDRESS))
        Boot_Partition(APP_B_ADDRESS, GPIO_Pin_13, GPIOE);

    /* (4) No valid APP: blink PE14 forever, accept 'x' cmd any time */
    BLE_PRINTF("[BOOT] 未发现有效的 APP 应用!\r\n");
    BLE_PRINTF("[BOOT] 随时可通过蓝牙发送 'x' + 回车 启动 OTA HTTP 测试.\r\n");
    while (1) {
        GPIO_ResetBits(GPIOE, GPIO_Pin_14); delay_ms(300);
        GPIO_SetBits(GPIOE, GPIO_Pin_14);   delay_ms(300);
        /* 持续监听蓝牙命令 */
        Process_RxCMD();
    }
}
