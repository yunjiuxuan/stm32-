#include "dsp_usart.h"
#include "stm32f4xx_flash.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* 内部分区与跳转标志定义 (与 main.c / key_task.c 一致) */
#define OTA_APP_A_ADDRESS      0x08040000U
#define OTA_APP_B_ADDRESS      0x080A0000U
#define OTA_JUMP_FLAG_TO_A     0xA5A50001U
#define OTA_JUMP_FLAG_TO_B     0xA5A50002U
#define OTA_UPGRADE_A          0xA5A50003U   /* APP长按: 当前运行B,请求升级到A后跳A */
#define OTA_UPGRADE_B          0xA5A50004U   /* APP长按: 当前运行A,请求升级到B后跳B */
#define OTA_LAST_BOOT_TO_A     0x5A5A0001U
#define OTA_LAST_BOOT_TO_B     0x5A5A0002U

uint8_t _bleflg = 0;

#define USART1_TX_GPIO_PIN		GPIO_Pin_9
#define USART1_RX_GPIO_PIN		GPIO_Pin_10
#define USART1_GPIO_PORT		GPIOA

/* ==================== OTA 三缓冲全局变量 (参考 OTA_bemfa.com 项目风格) ==================== */
/* 模式切换标志: 1=下载模式(ISR填三缓冲), 0=普通模式(ISR填recv_buff)  类似参考项目 DownloadFlag */
volatile uint8_t  g_ota_download_flag = 0;
/* 三缓冲: 每缓冲 4KB (对齐W25Q128扇区), ISR填一个, 主循环写另两个任意一个空闲的 */
#define OTA_DBUF_SIZE       4096U
uint8_t  g_ota_bufA[OTA_DBUF_SIZE];
uint8_t  g_ota_bufB[OTA_DBUF_SIZE];
uint8_t  g_ota_bufC[OTA_DBUF_SIZE];   /* 第3缓冲: ISR永远不会被迫原地覆盖 */
volatile uint8_t  *g_ota_rx_buf;      /* ISR当前正在填充的缓冲 */
volatile uint16_t  g_ota_rx_cnt;      /* 当前缓冲已写入字节数 */
volatile uint8_t   g_ota_bufA_full;   /* =1: 缓冲A已满,待主循环写Flash */
volatile uint8_t   g_ota_bufB_full;   /* =1: 缓冲B已满,待主循环写Flash */
volatile uint8_t   g_ota_bufC_full;   /* =1: 缓冲C已满,待主循环写Flash */
uint32_t  g_ota_w25q_addrP;           /* W25Q128写入地址指针, 类似参考项目 addrP */
volatile uint32_t g_ota_total_recv;   /* ISR累计接收字节数 (下载期间) */
volatile uint32_t g_ota_overflow_cnt; /* 缓冲溢出次数 (三缓冲均满才真溢出,基本为0) */

/* ==================== OTA 下载状态机 + 统计变量 (必须在 USART3_IRQHandler 之前定义) ==================== */
/* IPD 解析状态机 (非透传模式剥 "+IPD,len:" 前缀) */
typedef enum {
    IPD_ST_IDLE = 0,        /* 等待 '+' */
    IPD_ST_SKIP_PREFIX,     /* 跳过 'IPD,' 4个字符 */
    IPD_ST_LEN,             /* 读取长度数字 */
    IPD_ST_DATA,            /* 正在接收 payload 数据 */
} ipd_state_t;

static ipd_state_t g_ipd_state = IPD_ST_IDLE;
static unsigned long g_ipd_expect_len = 0;  /* 当前 IPD 包声明的数据长度 */
static unsigned long g_ipd_recv_len   = 0;  /* 当前 IPD 包已取走的数据长度 */
static uint8_t g_ipd_skip_cnt = 0;          /* 跳过 "IPD," 前缀的字符计数 */

/* ===== IPD 解析统计 (完全不影响下载: 仅计数, 下载结束后一次性打印) ===== */
static uint32_t g_st_ipd_pkt_ok      = 0;  /* 完整解析成功的 +IPD 包个数 */
static uint32_t g_st_ipd_pkt_badpre  = 0;  /* SKIP_PREFIX 匹配失败次数 (无+IPD,前缀) */
static uint32_t g_st_ipd_pkt_badlen  = 0;  /* LEN 阶段异常字符次数 (长度字段有非数字非:) */
static uint32_t g_st_ipd_pkt_empty   = 0;  /* 解析到 expect_len=0 的空包次数 */
static uint32_t g_st_ipd_idle_drop   = 0;  /* IDLE阶段丢弃的非'+'字节数 (AT回显/SEND OK等) */
static uint32_t g_st_ipd_max_len     = 0;  /* 观察到的最大单个IPD包长度 (用于诊断) */
static uint32_t g_st_ipd_min_len     = 0xFFFFFFFFU; /* 观察到的最小单个IPD包长度 */

/* HTTP 响应解析状态 */
typedef enum {
    HTTP_ST_HEADER = 0,   /* 正在接收响应头 */
    HTTP_ST_BODY,         /* 正在接收 body (固件) */
    HTTP_ST_DONE,         /* 接收完成 */
} http_state_t;

static http_state_t g_http_state = HTTP_ST_HEADER;
static uint16_t     g_header_buf_cnt = 0;
static uint8_t      g_header_buf[1024];    /* 暂存响应头 (最多1KB) */
static uint32_t     g_fw_offset = 0;       /* 固件在W25Q128中的偏移(HTTP头长度), 下载后解析 */

/* 下载过程全局状态 (Content-Length/W25Q写入偏移等) */
#define OTA_W25Q_FW_START_ADDR   0x00000000U   /* W25Q128 中固件存储起始地址 */
#define W25Q_SECTOR_SIZE         4096U         /* W25Q128 扇区大小 4KB */
#define W25Q_PAGE_SIZE           256U          /* W25Q128 页大小 256B */
#define W25Q_BLOCK_SIZE_64K      65536U        /* W25Q128 块大小 64KB (擦除比16x扇区快5倍) */

unsigned long g_bin_total_len = 0;    /* Content-Length: 固件总字节数 (extern供外部读) */
unsigned long g_bin_recv_len  = 0;    /* 已接收的固件 body 字节数 (extern供外部读) */
static uint32_t      g_w25q_write_addr = OTA_W25Q_FW_START_ADDR;  /* W25Q128 当前写入地址 */
static uint16_t      g_page_buf_cnt = 0;     /* 页缓冲已写入字节数 */
static uint8_t       g_page_buf[W25Q_PAGE_SIZE]; /* 页写缓冲，防止跨页 */

void PA9_10_USART1_Init(uint32_t _baud)
{
	GPIO_InitTypeDef 	GPIO_InitStructure;
	USART_InitTypeDef 	USART_InitStructure;
	NVIC_InitTypeDef	NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9,  GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	GPIO_InitStructure.GPIO_Pin 	= USART1_TX_GPIO_PIN | USART1_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd	= GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_OType	= GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed	= GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate		= _baud;
	USART_InitStructure.USART_WordLength	= USART_WordLength_8b;
	USART_InitStructure.USART_Parity		= USART_Parity_No;
	USART_InitStructure.USART_Mode			= USART_Mode_Rx | USART_Mode_Tx;
	USART_InitStructure.USART_StopBits		= USART_StopBits_1;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART1, &USART_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority		 = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd	= ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART1, ENABLE);
}

uint8_t _logflg = 0;
uint16_t data = 0;
uint8_t cmd[50] = {0};
uint8_t cmd_tmp[50];

void USART1_IRQHandler(void)
{
	static uint8_t count = 0;
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);

		data = USART_ReceiveData(USART1);
		if( data != '\n' )
			cmd_tmp[count++] = data;
		else
		{
			cmd_tmp[count-1] = '\0';
			memset(cmd, 0, sizeof(cmd));
			strncpy((char *)cmd, (char *)cmd_tmp, count-1);
			memset(cmd_tmp, 0, sizeof(cmd_tmp));
			count = 0;
			_logflg = 1;
		}
	}
}

#define USART2_TX_GPIO_PIN		GPIO_Pin_2
#define USART2_RX_GPIO_PIN		GPIO_Pin_3
#define USART2_GPIO_PORT		GPIOA

void LOG_USART2_SEND(char *msg)
{
    uint8_t len = strlen(msg);
    for(uint8_t i = 0; i<len; i++)
    {
        USART_SendData(USART2, msg[i]);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    }
}

void BLE_PRINTF(const char *fmt, ...)
{
	char ble_buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(ble_buf, sizeof(ble_buf), fmt, args);
	va_end(args);
	LOG_USART2_SEND(ble_buf);
}

void Boot_Print(const char *fmt, ...)
{
	char ble_buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(ble_buf, sizeof(ble_buf), fmt, args);
	va_end(args);
	LOG_USART2_SEND(ble_buf);
}

void PA2_3_USART2_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

	GPIO_InitStructure.GPIO_Pin = USART2_TX_GPIO_PIN | USART2_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART2, &USART_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART2, ENABLE);
}

uint8_t BLE_String_tmp[64];
uint8_t BLE_String[64] = {0};
uint8_t ble_rx_count = 0;

void USART2_IRQHandler(void)
{
	static uint8_t u2_count = 0;

	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
	{
		uint8_t rx_data = USART_ReceiveData(USART2);
		ble_rx_count++;

		if(rx_data == '\n')
		{
			if(u2_count > 0)
			{
				if(BLE_String_tmp[u2_count-1] == '\r')
					BLE_String_tmp[u2_count-1] = '\0';
				else
					BLE_String_tmp[u2_count] = '\0';

				strncpy((char *)BLE_String, (char *)BLE_String_tmp, sizeof(BLE_String)-1);
				BLE_String[sizeof(BLE_String)-1] = '\0';

				_bleflg = 1;
			}

			memset(BLE_String_tmp, 0, sizeof(BLE_String_tmp));
			u2_count = 0;
		}
		else if(u2_count < sizeof(BLE_String_tmp)-1)
		{
			BLE_String_tmp[u2_count++] = rx_data;
		}
		else
		{
			memset(BLE_String_tmp, 0, sizeof(BLE_String_tmp));
			u2_count = 0;
		}
	}
}

#include "delay.h"

#define WIFI_USERNAME		"yjxhm"
#define WIFI_PASSWORD		"nshnshnsh"
#define UID					"3f2d5feab40ea3844d23321f165ab2db"
#define TOPIC_1				"ota"

#define ESP8266_PIN_TX			GPIO_Pin_10
#define ESP8266_PIN_RX			GPIO_Pin_11
#define ESP8266_GPIOx_PORT		GPIOB
#define ESP8266_GPIOx_RCC		RCC_AHB1Periph_GPIOB
#define ESP8266_USART_RCC		RCC_APB1Periph_USART3
#define ESP8266_UART_RCCAPB1	1

void PB10_11_ESP8266_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(ESP8266_GPIOx_RCC, ENABLE);

	#if ESP8266_UART_RCCAPB1
		RCC_APB1PeriphClockCmd(ESP8266_USART_RCC, ENABLE);
	#else
		RCC_APB2PeriphClockCmd(ESP8266_USART_RCC, ENABLE);
	#endif

	GPIO_InitStructure.GPIO_Pin = ESP8266_PIN_TX | ESP8266_PIN_RX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(ESP8266_GPIOx_PORT, &GPIO_InitStructure);

	GPIO_PinAFConfig(ESP8266_GPIOx_PORT, GPIO_PinSource10, GPIO_AF_USART3);
	GPIO_PinAFConfig(ESP8266_GPIOx_PORT, GPIO_PinSource11, GPIO_AF_USART3);

	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART3, &USART_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART3, ENABLE);
}

uint8_t u3_count = 0;
#define RX_BUFF_SIZE			2048
char recv_buff[RX_BUFF_SIZE] = {0};

uint8_t WIFI_String[256];
uint8_t wifi_flag = 0;
static char wifi_tmp[256];
static uint8_t wifi_tmp_cnt = 0;

void USART3_IRQHandler(void)
{
	char data;
	if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
	{
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);

		data = USART_ReceiveData(USART3);

		/* ===== 下载模式 (非透传: IPD状态机剥前缀 -> 三缓冲) ===== */
		if (g_ota_download_flag)
		{
			/* ============== IPD 状态机: 剥 "+IPD,len:" 前缀 ==============
			 * 非透传模式下ESP8266包格式: +IPD,1234:<payload 1234字节>
			 * 可能连续多个IPD包, 也可能包之间夹杂AT回显/状态行(需丢弃)
			 *   注意: 统计变量纯计数, 无打印无阻塞, 不影响下载实时性
			 */
			switch (g_ipd_state)
			{
			case IPD_ST_IDLE:
				/* 等待 '+' 开头, 其他字节(如AT回显/SEND OK/CLOSED等)直接丢弃 */
				if (data == '+')
				{
					g_ipd_skip_cnt = 0;
					g_ipd_state = IPD_ST_SKIP_PREFIX;
				}
				else
				{
					g_st_ipd_idle_drop++;  /* IDLE非'+'一律丢弃(统计) */
				}
				break;

			case IPD_ST_SKIP_PREFIX:
				/* 匹配 "IPD," 4个字符 */
				if (g_ipd_skip_cnt == 0)
				{
					if (data == 'I') { g_ipd_skip_cnt = 1; }
					else { g_st_ipd_pkt_badpre++; g_ipd_state = IPD_ST_IDLE; }
				}
				else if (g_ipd_skip_cnt == 1)
				{
					if (data == 'P') { g_ipd_skip_cnt = 2; }
					else { g_st_ipd_pkt_badpre++; g_ipd_state = IPD_ST_IDLE; }
				}
				else if (g_ipd_skip_cnt == 2)
				{
					if (data == 'D') { g_ipd_skip_cnt = 3; }
					else { g_st_ipd_pkt_badpre++; g_ipd_state = IPD_ST_IDLE; }
				}
				else /* skip_cnt == 3, 期待 ',' */
				{
					if (data == ',')
					{
						g_ipd_expect_len = 0;
						g_ipd_recv_len = 0;
						g_ipd_state = IPD_ST_LEN;
					}
					else { g_st_ipd_pkt_badpre++; g_ipd_state = IPD_ST_IDLE; }
				}
				break;

			case IPD_ST_LEN:
				/* 读取长度数字, 直到 ':' 分隔符 */
				if (data >= '0' && data <= '9')
				{
					g_ipd_expect_len = g_ipd_expect_len * 10 + (uint32_t)(data - '0');
				}
				else if (data == ':')
				{
					/* 长度解析完毕, 进入DATA阶段 */
					if (g_ipd_expect_len == 0)
					{
						g_st_ipd_pkt_empty++;
						g_ipd_state = IPD_ST_IDLE;  /* 0长度包回IDLE重找下一包 */
					}
					else
					{
						/* 统计最大/最小包长 */
						if (g_ipd_expect_len > g_st_ipd_max_len) g_st_ipd_max_len = (uint32_t)g_ipd_expect_len;
						if (g_ipd_expect_len < g_st_ipd_min_len) g_st_ipd_min_len = (uint32_t)g_ipd_expect_len;
						g_ipd_state = IPD_ST_DATA;
					}
				}
				else
				{
					/* 长度字段异常字符, 回IDLE */
					g_st_ipd_pkt_badlen++;
					g_ipd_state = IPD_ST_IDLE;
				}
				break;

			case IPD_ST_DATA:
			{
				/* DATA阶段: 本字节写入三缓冲 (HTTP响应头 + 固件body) */
				g_ota_rx_buf[g_ota_rx_cnt++] = (uint8_t)data;
				g_ota_total_recv++;
				g_ipd_recv_len++;

				if (g_ota_rx_cnt >= OTA_DBUF_SIZE)
				{
					/* 当前缓冲(4KB)满: 先标记当前缓冲满, 再轮转找下一个空闲缓冲 */
					uint8_t next_ok = 0;
					if (g_ota_rx_buf == g_ota_bufA)
					{
						g_ota_bufA_full = 1;
						if (!g_ota_bufB_full)      { g_ota_rx_buf = g_ota_bufB; next_ok = 1; }
						else if (!g_ota_bufC_full) { g_ota_rx_buf = g_ota_bufC; next_ok = 1; }
					}
					else if (g_ota_rx_buf == g_ota_bufB)
					{
						g_ota_bufB_full = 1;
						if (!g_ota_bufC_full)      { g_ota_rx_buf = g_ota_bufC; next_ok = 1; }
						else if (!g_ota_bufA_full) { g_ota_rx_buf = g_ota_bufA; next_ok = 1; }
					}
					else /* 当前填C */
					{
						g_ota_bufC_full = 1;
						if (!g_ota_bufA_full)      { g_ota_rx_buf = g_ota_bufA; next_ok = 1; }
						else if (!g_ota_bufB_full) { g_ota_rx_buf = g_ota_bufB; next_ok = 1; }
					}

					if (next_ok)
					{
						g_ota_rx_cnt = 0;
					}
					else
					{
						/* 三缓冲全满: 溢出 */
						g_ota_overflow_cnt++;
						g_ota_rx_cnt = 0;
					}
				}

				/* 本IPD包收完了? -> 回IDLE等下一个+IPD */
				if (g_ipd_recv_len >= g_ipd_expect_len)
				{
					g_st_ipd_pkt_ok++;  /* 一个完整的+IPD包解析成功 */
					g_ipd_state = IPD_ST_IDLE;
				}
				break;
			} /* case IPD_ST_DATA */

			default:
				g_ipd_state = IPD_ST_IDLE;
				break;
			}
			return; /* 下载模式不走下面的普通 recv_buff / wifi_tmp 处理 */
		}

		/* ===== 普通模式: 填 recv_buff + wifi 行解析 ===== */
		if(u3_count < RX_BUFF_SIZE - 1)
		{
			recv_buff[u3_count++] = data;
			recv_buff[u3_count] = '\0';
		}
		else
		{
			u3_count = 0;
			recv_buff[0] = data;
			recv_buff[1] = '\0';
			u3_count = 1;
		}

		if(data == '\n')
		{
			if(wifi_tmp_cnt > 0)
			{
				if(wifi_tmp[wifi_tmp_cnt-1] == '\r')
					wifi_tmp[wifi_tmp_cnt-1] = '\0';
				else
					wifi_tmp[wifi_tmp_cnt] = '\0';

				if(strstr(wifi_tmp, "msg=") != NULL && strstr(wifi_tmp, "msg=OK") == NULL && strstr(wifi_tmp, "msg=FAIL") == NULL)
				{
					strncpy((char *)WIFI_String, wifi_tmp, sizeof(WIFI_String)-1);
					wifi_flag = 1;
				}
			}
			memset(wifi_tmp, 0, sizeof(wifi_tmp));
			wifi_tmp_cnt = 0;
		}
		else if(wifi_tmp_cnt < sizeof(wifi_tmp)-1)
		{
			wifi_tmp[wifi_tmp_cnt++] = data;
		}
	}

	/* 处理溢出错误(ORE): 禁用中断期间数据丢失会导致ORE, 读SR再读DR清除 */
	if(USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET)
	{
		(void)USART3->SR;
		(void)USART3->DR;
	}
}

int Parse_Bafa_Msg(char *raw, char *msg_out, uint16_t out_len)
{
	char *p_start;
	uint16_t msg_len;

	p_start = strstr(raw, "msg=");
	if(p_start == NULL)
		return -1;

	p_start += 4;

	msg_len = strlen(p_start);

	if(msg_len > 0 && p_start[msg_len-1] == '\r')
		msg_len--;

	if(msg_len >= out_len)
		msg_len = out_len - 1;

	strncpy(msg_out, p_start, msg_len);
	msg_out[msg_len] = '\0';

	return 0;
}

 

void ESP8266_Send_AT(const char *AT_MSG)
{
	uint8_t i;
	for(i=0; i<strlen(AT_MSG); i++)
	{
		USART_SendData(USART3, AT_MSG[i]);
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	}
}

int ESP8266_Recv_ACK(const char *AT_ACK, int timeout)
{
	int cnt=0,ret = -1;
	while(1)
	{
		if(strstr(recv_buff,AT_ACK) != NULL)
		{
			ret = 0;
			break;
		}
		else
		{
			delay_ms(10);
			cnt++;
			if(cnt>(timeout/10))
			{
				ret = -1;
				break;
			}
		}
	}
	BLE_PRINTF("[AT] 期待:'%s' | 结果:%s (超时=%dms)\r\n", AT_ACK, (ret==0)?"成功":"超时", timeout);
	u3_count = 0;
	memset(recv_buff,0,RX_BUFF_SIZE);
	return ret;
}

int ESP8266_Connect_Server(void)
{
	delay_s(1);
	ESP8266_Send_AT("+++");
	delay_s(1);

	ESP8266_Send_AT("AT\r\n");
	int ack_ret = ESP8266_Recv_ACK("OK", 3000);
	if(ack_ret == -1)
	{
		return -1;
	}

	ESP8266_Send_AT("AT+CWMODE=3\r\n");
	ack_ret = ESP8266_Recv_ACK("OK", 3000);
	if(ack_ret == -1)
	{
		return -1;
	}

	char WIFI_PASS[64] = {0};
	sprintf(WIFI_PASS,"AT+CWJAP=\"%s\",\"%s\"\r\n",WIFI_USERNAME, WIFI_PASSWORD);
	ESP8266_Send_AT(WIFI_PASS);
	if(ESP8266_Recv_ACK("OK", 8000) == -1)
	{
		return -1;
	}

	ESP8266_Send_AT("AT+CIPMODE=1\r\n");
	if(ESP8266_Recv_ACK("OK", 3000) == -1)
	{
		return -1;
	}

	ESP8266_Send_AT("AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n");
	if(ESP8266_Recv_ACK("OK", 10000) == -1)
	{
		return -1;
	}

	ESP8266_Send_AT("AT+CIPSEND\r\n");
	if(ESP8266_Recv_ACK(">", 10000) == -1)
	{
		return -1;
	}

	char topic[128] = {0};
	sprintf(topic,"cmd=1&uid=%s&topic=%s\r\n",UID,TOPIC_1);
	ESP8266_Send_AT(topic);
	if(ESP8266_Recv_ACK("cmd=1&res=1", 3000) == -1)
	{
		return -1;
	}

	return 0;
}

void OTA_LOG(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Boot_Print(buf);
    BLE_PRINTF("%s", buf);
}

#define OTA_HTTP_HOST       "bin.bemfa.com"
#define OTA_HTTP_PORT       80
//https://apis.bemfa.com/vb/api/v1/firmwareVersion?openID=3f2d5feab40ea3844d23321f165ab2db&topic=ota&deviceType=3
#define OTA_META_HOST       "apis.bemfa.com"
#define OTA_META_PORT       80
#define OTA_META_PATH       "/vb/api/v1/firmwareVersion?openID=3f2d5feab40ea3844d23321f165ab2db&topic=ota&deviceType=3"
#define OTA_META_RESP_SIZE  1024
#define OTA_BIN_RESP_SIZE   1024

typedef struct {
    int code;
    char msg[32];
    char url[160];
    int version;
    char tag[32];
    unsigned long size;
    unsigned long long unix;
    uint32_t crc32;
} ota_meta_t;

static int _OTA_InitESP8266(void)
{
    int ret;

    BLE_PRINTF("[OTA]   退出透传模式 (间隔1s 发 +++)\r\n");
    delay_s(1);
    ESP8266_Send_AT("+++");
    delay_s(1);

    ESP8266_Send_AT("AT\r\n");
    ret = ESP8266_Recv_ACK("OK", 3000);
    if (ret != 0)
    {
        BLE_PRINTF("[OTA]   警告: AT无响应, 可能已在非透传模式\r\n");
    }

    ESP8266_Send_AT("AT+CIPCLOSE\r\n");
    ESP8266_Recv_ACK("OK", 2000);

    BLE_PRINTF("[OTA]   WiFi连接 SSID='%s'\r\n", WIFI_USERNAME);
    ESP8266_Send_AT("AT+CWMODE=3\r\n");
    if (ESP8266_Recv_ACK("OK", 3000) != 0)
    {
        BLE_PRINTF("[OTA]   错误: CWMODE=3 设置失败\r\n");
        return -1;
    }

    char WIFI_PASS[64];
    sprintf(WIFI_PASS, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_USERNAME, WIFI_PASSWORD);
    ESP8266_Send_AT(WIFI_PASS);
    if (ESP8266_Recv_ACK("OK", 8000) != 0)
    {
        BLE_PRINTF("[OTA]   错误: WiFi连接失败\r\n");
        return -1;
    }
    BLE_PRINTF("[OTA]   WiFi连接成功\r\n");

    ESP8266_Send_AT("AT+CIPMUX=0\r\n");
    ESP8266_Recv_ACK("OK", 2000);
    ESP8266_Send_AT("AT+CIPMODE=0\r\n");
    if (ESP8266_Recv_ACK("OK", 2000) != 0)
    {
        BLE_PRINTF("[OTA]   错误: CIPMODE=0 设置失败\r\n");
        return -1;
    }
    BLE_PRINTF("[OTA]   ESP8266 就绪 (非透传命令模式)\r\n");
    return 0;
}

static int _OTA_HTTP_Get(const char *host, int port, const char *path,
                          char *resp_buff, uint16_t resp_size, int recv_timeout_ms,
                          int use_ssl)
{
    int ret;

    char tcpcmd[64];
    if (use_ssl)
    {
        BLE_PRINTF("[OTA]   开启 SSL 模式 (AT+CIPSSL=1)\r\n");
        ESP8266_Send_AT("AT+CIPSSL=1\r\n");
        ret = ESP8266_Recv_ACK("OK", 5000);
        BLE_PRINTF("[OTA]   SSL 模式: %s\r\n", ret == 0 ? "OK" : "失败(将尝试继续)");
    }

    if (use_ssl)
        sprintf(tcpcmd, "AT+CIPSTART=\"SSL\",\"%s\",%d\r\n", host, port);
    else
        sprintf(tcpcmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", host, port);

    BLE_PRINTF("[OTA]   发送: %s", tcpcmd);
    ESP8266_Send_AT(tcpcmd);
    ret = ESP8266_Recv_ACK("CONNECT", 15000);
    BLE_PRINTF("[OTA]   连接 %s:%d -> %s\r\n", host, port, ret == 0 ? "成功" : "失败");
    if (ret != 0)
    {
        BLE_PRINTF("[OTA]   错误: 无法连接到 %s:%d\r\n", host, port);
        return -1;
    }

    char http_req[300];
    int  req_len = sprintf(http_req,
                           "GET %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "User-Agent: ESP8266\r\n"
                           "Accept: */*\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           path, host);

    BLE_PRINTF("[OTA]   HTTP 请求 (%d字节):\r\n", req_len);
    BLE_PRINTF("%s", http_req);

    char cipcmd[32];
    sprintf(cipcmd, "AT+CIPSEND=%d\r\n", req_len);
    ESP8266_Send_AT(cipcmd);
    ret = ESP8266_Recv_ACK(">", 5000);
    BLE_PRINTF("[OTA]   CIPSEND: %s\r\n", ret == 0 ? "就绪" : "超时");
    if (ret != 0)
    {
        BLE_PRINTF("[OTA]   错误: CIPSEND 失败\r\n");
        ESP8266_Send_AT("AT+CIPCLOSE\r\n");
        return -1;
    }
    ESP8266_Send_AT(http_req);
    BLE_PRINTF("[OTA]   请求已发送, 接收数据(不打印,收完再打印)...\r\n");

    u3_count = 0;

    {
        int loop_cnt = 0;
        int total = 0;
        int wait_cnt = 0;
        int got_data = 0;

        resp_buff[0] = '\0';

        while (1)
        {
            int avail = u3_count;
            if (avail > 0)
            {
                if (avail > (int)resp_size - 1 - total)
                    avail = (int)resp_size - 1 - total;

                if (avail > 0)
                {
                    memcpy(resp_buff + total, recv_buff, avail);
                    total += avail;
                    resp_buff[total] = '\0';
                    got_data = 1;
                }
                u3_count = 0;
                wait_cnt = 0;
            }

            if (got_data && wait_cnt > 20)
                break;

            delay_ms(5);
            loop_cnt++;

            if (u3_count > 0) wait_cnt = 0;
            else if (got_data) wait_cnt++;

            if (loop_cnt > (recv_timeout_ms / 5))
                break;
        }

        BLE_PRINTF("\r\n[OTA]   === 接收完成, 共 %d 字节 ===\r\n", total);
        {
            int print_len = total;
            int offset = 0;
            int seg = 0;
            while (offset < print_len)
            {
                char seg_buf[201];
                int chunk = print_len - offset;
                if (chunk > 200) chunk = 200;
                memcpy(seg_buf, resp_buff + offset, chunk);
                seg_buf[chunk] = '\0';
                BLE_PRINTF("[seg%d] %s\r\n", seg, seg_buf);
                offset += chunk;
                seg++;
                delay_ms(80);
            }
            BLE_PRINTF("[OTA]   === 打印完成, 共 %d 段 ===\r\n", seg);
        }
    }

    ESP8266_Send_AT("AT+CIPCLOSE\r\n");
    ESP8266_Recv_ACK("OK", 2000);
    return 0;
}

static int _OTA_ParseJsonMetadata(const char *json, ota_meta_t *meta)
{
    char *p;
    meta->code = -1;
    meta->msg[0] = '\0';
    meta->url[0] = '\0';
    meta->version = 0;
    meta->tag[0] = '\0';
    meta->size = 0;
    meta->unix = 0;
    meta->crc32 = 0;

    p = strstr(json, "\"code\":");
    if (p != NULL)
        sscanf(p + 7, "%d", &meta->code);

    p = strstr(json, "\"msg\":\"");
    if (p != NULL)
    {
        p += 7;
        int i;
        for (i = 0; i < 31 && p[i] != '"'; i++)
            meta->msg[i] = p[i];
        meta->msg[i] = '\0';
    }

    p = strstr(json, "\"data\":");
    if (p != NULL)
    {
        p += 7;
        p = strchr(p, '{');
        if (p != NULL)
        {
            char *url_p = strstr(p, "\"url\":\"");
            if (url_p != NULL)
            {
                url_p += 7;
                int i;
                for (i = 0; i < 159 && url_p[i] != '"'; i++)
                    meta->url[i] = url_p[i];
                meta->url[i] = '\0';
            }

            char *ver_p = strstr(p, "\"version\":");
            if (ver_p != NULL)
                sscanf(ver_p + 10, "%d", &meta->version);

            char *tag_p = strstr(p, "\"tag\":\"");
            if (tag_p != NULL)
            {
                tag_p += 7;
                int i;
                for (i = 0; i < 31 && tag_p[i] != '"'; i++)
                    meta->tag[i] = tag_p[i];
                meta->tag[i] = '\0';
            }

            char *size_p = strstr(p, "\"size\":");
            if (size_p != NULL)
                sscanf(size_p + 7, "%lu", (unsigned long *)&meta->size);

            char *unix_p = strstr(p, "\"unix\":");
            if (unix_p != NULL)
                sscanf(unix_p + 7, "%llu", &meta->unix);

            char *crc_p = strstr(p, "\"crc32\":");
            if (crc_p != NULL)
                sscanf(crc_p + 8, "%u", (unsigned int *)&meta->crc32);
        }
    }
    else
    {
        p = strstr(json, "\"url\":\"");
        if (p != NULL)
        {
            p += 7;
            int i;
            for (i = 0; i < 159 && p[i] != '"'; i++)
                meta->url[i] = p[i];
            meta->url[i] = '\0';
        }

        p = strstr(json, "\"version\":");
        if (p != NULL)
            sscanf(p + 10, "%d", &meta->version);

        p = strstr(json, "\"tag\":\"");
        if (p != NULL)
        {
            p += 7;
            int i;
            for (i = 0; i < 31 && p[i] != '"'; i++)
                meta->tag[i] = p[i];
            meta->tag[i] = '\0';
        }

        p = strstr(json, "\"size\":");
        if (p != NULL)
            sscanf(p + 7, "%lu", (unsigned long *)&meta->size);

        p = strstr(json, "\"unix\":");
        if (p != NULL)
            sscanf(p + 7, "%llu", &meta->unix);

        p = strstr(json, "\"crc32\":");
        if (p != NULL)
            sscanf(p + 8, "%u", (unsigned int *)&meta->crc32);
    }

    if (meta->code == 0 && strlen(meta->url) > 0)
        return 0;
    return -1;
}

static int _OTA_ParseHttpHeaders(const char *resp, int *status,
                                  unsigned long *content_length,
                                  char *content_type, int ct_size,
                                  char **body_start)
{
    char *p;
    *status = 0;
    *content_length = 0;
    content_type[0] = '\0';
    *body_start = NULL;

    p = strstr(resp, "HTTP/");
    if (p != NULL)
    {
        sscanf(p, "HTTP/1.1 %d", status);
    }

    p = strstr(resp, "Content-Length:");
    if (p != NULL)
    {
        sscanf(p + 16, "%lu", (unsigned long *)content_length);
    }

    p = strstr(resp, "Content-Type:");
    if (p != NULL)
    {
        char *pend = strstr(p, "\r\n");
        if (pend != NULL)
        {
            int len = (int)(pend - (p + 14));
            if (len > ct_size - 1) len = ct_size - 1;
            memcpy(content_type, p + 14, len);
            content_type[len] = '\0';
        }
    }

    p = strstr(resp, "\r\n\r\n");
    if (p != NULL)
    {
        *body_start = p + 4;
        return 0;
    }
    return -1;
}

/* ========== OTA 固件下载到 W25Q128 ========== */
/* (注意: 宏/全局状态/IPD状态机/HTTP状态均已移到文件前部, ISR之前) */

/*
 * 函数: _OTA_FlushPageBuf
 * 用途: 将页缓冲中的数据写入 W25Q128，并清零计数
 * 注意: 调用者必须保证 len <= W25Q_PAGE_SIZE，且不跨页
 */
static void _OTA_FlushPageBuf(void)
{
    if (g_page_buf_cnt == 0)
        return;

    w25q128_WritePageData(g_w25q_write_addr, g_page_buf, g_page_buf_cnt);
    g_w25q_write_addr += g_page_buf_cnt;
    g_page_buf_cnt = 0;
}

/*
 * 函数: _OTA_WriteByteToFlash
 * 用途: 单字节写入固件到 W25Q128，自动处理页边界
 */
static void _OTA_WriteByteToFlash(uint8_t byte)
{
    if (g_page_buf_cnt >= W25Q_PAGE_SIZE)
    {
        _OTA_FlushPageBuf();  /* 页满了，刷到 Flash */
    }
    g_page_buf[g_page_buf_cnt++] = byte;
    g_bin_recv_len++;
}

/*
 * 函数: _OTA_ParseHeaderAndSwitchToBody
 * 用途: 解析响应头，提取 Content-Length，切换到 body 接收
 * 返回: 0=成功 -1=失败
 */
static int _OTA_ParseHeaderAndSwitchToBody(void)
{
    int    http_status = 0;
    unsigned long content_length = 0;
    char   content_type[64];
    char  *body_ptr = NULL;
    int    ret;

    g_header_buf[g_header_buf_cnt] = '\0';

    ret = _OTA_ParseHttpHeaders((char *)g_header_buf, &http_status,
                                &content_length, content_type,
                                sizeof(content_type), &body_ptr);
    if (ret != 0)
        return -1;

    if (http_status != 200)
        return -1;

    if (content_length == 0)
        return -1;

    g_bin_total_len = content_length;

    /* 检查 body_ptr 是否指向 header_buf 内的数据（响应头末尾拼接的 body 开头字节） */
    if (body_ptr != NULL)
    {
        uint32_t pre_bytes = (uint32_t)((uint8_t *)g_header_buf + g_header_buf_cnt - (uint8_t *)body_ptr);
        if (pre_bytes > 0)
        {
            if (pre_bytes > content_length)
                pre_bytes = (uint32_t)content_length;
            for (uint32_t i = 0; i < pre_bytes; i++)
            {
                _OTA_WriteByteToFlash(body_ptr[i]);
            }
        }
    }

    g_http_state = HTTP_ST_BODY;
    return 0;
}

/*
 * 函数: _OTA_FeedHeaderByte
 * 用途: HTTP 响应头阶段喂入一个字节，检测 \r\n\r\n 结束符
 */
static int _OTA_FeedHeaderByte(uint8_t b)
{
    if (g_header_buf_cnt < sizeof(g_header_buf) - 1)
    {
        g_header_buf[g_header_buf_cnt++] = b;
    }
    else
    {
        return -1;
    }

    /* 检测是否出现 \r\n\r\n */
    if (g_header_buf_cnt >= 4)
    {
        uint16_t tail = g_header_buf_cnt - 4;
        if (g_header_buf[tail]   == '\r' &&
            g_header_buf[tail+1] == '\n' &&
            g_header_buf[tail+2] == '\r' &&
            g_header_buf[tail+3] == '\n')
        {
            return _OTA_ParseHeaderAndSwitchToBody();
        }
    }
    return 0;
}

/*
 * 函数: _OTA_IPD_FeedByte
 * 用途: IPD 状态机喂入一个字节，剥除 '+IPD,len:' 前缀，提取纯数据
 *       ESP8266 非透传模式格式: +IPD,<len>:<data>
 *       需要先跳过 '+' 后的 'I','P','D',',' 4个字符，再读数字，再遇 ':' 进入数据
 * 返回: 0=继续(非数据字节) 1=数据有效(*out_byte已填充)
 */
static int _OTA_IPD_FeedByte(uint8_t b, uint8_t *out_byte)
{
    static const char ipd_prefix[] = "IPD,";

    switch (g_ipd_state)
    {
        case IPD_ST_IDLE:
            /* 等待 '+' (AT响应如 OK/SEND OK/CLOSED 等全部丢弃) */
            if (b == '+')
            {
                g_ipd_state = IPD_ST_SKIP_PREFIX;
                g_ipd_skip_cnt = 0;
            }
            return 0;

        case IPD_ST_SKIP_PREFIX:
            /* 逐字符匹配 "IPD," 4个字符 */
            if (b == ipd_prefix[g_ipd_skip_cnt])
            {
                g_ipd_skip_cnt++;
                if (g_ipd_skip_cnt >= 4)
                {
                    /* "IPD," 匹配完毕，进入读数字阶段 */
                    g_ipd_state = IPD_ST_LEN;
                    g_ipd_expect_len = 0;
                }
            }
            else
            {
                /* 不匹配，回到 IDLE 重新等 '+' */
                BLE_PRINTF("[OTA]   IPD前缀不匹配: 期望'%c' 实际'%c'(0x%02X), 回到IDLE\r\n",
                           ipd_prefix[g_ipd_skip_cnt], (char)b, b);
                g_ipd_state = IPD_ST_IDLE;
            }
            return 0;

        case IPD_ST_LEN:
            /* 收集数字直到遇到 ':' */
            if (b >= '0' && b <= '9')
            {
                g_ipd_expect_len = g_ipd_expect_len * 10 + (b - '0');
            }
            else if (b == ':')
            {
                /* 长度读取完毕，进入数据阶段 */
                g_ipd_recv_len = 0;
                if (g_ipd_expect_len == 0)
                {
                    g_ipd_state = IPD_ST_IDLE;
                }
                else
                {
                    BLE_PRINTF("[OTA]   +IPD 包开始: 声明长度=%lu 字节\r\n", g_ipd_expect_len);
                    g_ipd_state = IPD_ST_DATA;
                }
            }
            else
            {
                /* 异常字符，回到 IDLE */
                BLE_PRINTF("[OTA]   IPD长度解析异常: '%c'(0x%02X), 回到IDLE\r\n", (char)b, b);
                g_ipd_state = IPD_ST_IDLE;
            }
            return 0;

        case IPD_ST_DATA:
            *out_byte = b;
            g_ipd_recv_len++;
            if (g_ipd_recv_len >= g_ipd_expect_len)
            {
                /* 当前 IPD 包收完，回到 IDLE 等下一个 */
                BLE_PRINTF("[OTA]   +IPD 包完成: 收到%lu/%lu字节, 回到IDLE\r\n",
                           g_ipd_recv_len, g_ipd_expect_len);
                g_ipd_state = IPD_ST_IDLE;
            }
            return 1;

        default:
            g_ipd_state = IPD_ST_IDLE;
            return 0;
    }
}

/*
 * 函数: _OTA_ProcessRecvBuffer
 * 用途: 处理 USART3 接收到的数据 (recv_buff 中 u3_count 字节)
 *       透传模式: recv_buff 中是原始 HTTP 响应 (无 +IPD 前缀)
 *       按 HTTP header/body 分流: header 检测 \r\n\r\n, body 写入 W25Q128
 */
static void _OTA_ProcessRecvBuffer(void)
{
    uint16_t i;
    uint16_t n = u3_count;
    uint16_t chunk;

    if (n == 0)
        return;

    /* 每次最多处理一半缓冲区, 防止处理期间新数据导致溢出 */
    chunk = (n > RX_BUFF_SIZE / 2) ? (RX_BUFF_SIZE / 2) : n;

    /* 处理数据时不禁用中断, 中断继续把新数据追加到 recv_buff 末尾 */
    for (i = 0; i < chunk; i++)
    {
        uint8_t raw = (uint8_t)recv_buff[i];

        if (g_http_state == HTTP_ST_DONE)
            break;

        if (g_http_state == HTTP_ST_BODY)
        {
            _OTA_WriteByteToFlash(raw);
            if (g_bin_total_len > 0 && g_bin_recv_len >= g_bin_total_len)
            {
                _OTA_FlushPageBuf();
                g_http_state = HTTP_ST_DONE;
                break;
            }
        }
    }

    /* 短暂禁用中断, 把未处理的数据移到缓冲区开头 */
    USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
    if (u3_count > chunk)
    {
        uint16_t remaining = u3_count - chunk;
        memmove(recv_buff, recv_buff + chunk, remaining);
        u3_count = remaining;
    }
    else
    {
        u3_count = 0;
    }
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}

/*
 * 函数: codeDownload
 * 用途: 从给定的 URL 下载 bin 固件到 W25Q128
 * 参数: url  固件下载地址 (如 http://bin.bemfa.com/xxx/xxx.bin)
 * 返回: 0=成功 -1=失败
 * 输出:
 *   - 固件数据存储在 W25Q128 的 OTA_W25Q_FW_START_ADDR 起始处
 *   - 下载字节数 / Content-Length 保存在 g_bin_recv_len / g_bin_total_len
 */
int codeDownload(const char *url)
{
    char bin_host[96];
    char bin_path[256];
    int  bin_port = 80;
    int  bin_ssl  = 0;
    const char *purl = url;
    int  host_i = 0;
    int  path_i = 0;
    int  ret;
    unsigned long last_print_len = 0;

    BLE_PRINTF("\r\n\r\n[OTA] ======= 步骤4: 固件下载 (codeDownload) =======\r\n");
    BLE_PRINTF("[OTA]   下载URL: %s\r\n", url);

    if (url == NULL || strlen(url) < 10)
    {
        BLE_PRINTF("[OTA] 错误: URL 为空或过短\r\n");
        return -1;
    }

    /* ------ 4.1 解析 URL ------ */
    BLE_PRINTF("[OTA] 4.1 解析 URL...\r\n");
    if (strncmp(purl, "https://", 8) == 0)
    {
        purl += 8;
        bin_port = 443;
        bin_ssl  = 1;
        BLE_PRINTF("[OTA]   检测到 HTTPS (可能不支持, 降级尝试)\r\n");
    }
    else if (strncmp(purl, "http://", 7) == 0)
    {
        purl += 7;
        bin_port = 80;
        bin_ssl  = 0;
    }

    /* 提取 host (到 '/' 或 ':' 为止，兼容 host:port 格式) */
    while (*purl && *purl != '/' && *purl != ':' && host_i < (int)sizeof(bin_host)-1)
        bin_host[host_i++] = *purl++;
    bin_host[host_i] = '\0';

    /* 如果是 ':' 开头，解析端口号 */
    if (*purl == ':')
    {
        purl++;  /* skip ':' */
        bin_port = 0;
        while (*purl >= '0' && *purl <= '9')
        {
            bin_port = bin_port * 10 + (*purl - '0');
            purl++;
        }
    }

    /* 剩下的都是 path */
    while (*purl && path_i < (int)sizeof(bin_path)-1)
        bin_path[path_i++] = *purl++;
    bin_path[path_i] = '\0';

    /* 如果 path 为空，默认 '/' */
    if (path_i == 0)
    {
        bin_path[0] = '/';
        bin_path[1] = '\0';
    }

    BLE_PRINTF("[OTA]   host=%s port=%d ssl=%d\r\n", bin_host, bin_port, bin_ssl);
    BLE_PRINTF("[OTA]   path=%s\r\n", bin_path);

    /* ------ 4.2 W25Q128 初始化 & 64KB块擦除 (比扇区擦除快约5倍) ------ */
    BLE_PRINTF("[OTA] 4.2 初始化 W25Q128 并擦除(64KB块擦除)...\r\n");
    w25q128_init();
    {
        uint16_t mid = w25q128_readID();
        BLE_PRINTF("[OTA]   W25Q128 ID: 0x%04X (expect EF40/EF18)\r\n", mid);
    }
    {
        unsigned long blocks_needed = 8;  /* 默认擦 8x64KB = 512KB, 足够384KB APP */
        if (g_bin_total_len > 0)
            blocks_needed = (g_bin_total_len + W25Q_BLOCK_SIZE_64K - 1) / W25Q_BLOCK_SIZE_64K;
        if (blocks_needed > 16) blocks_needed = 16;  /* 最多擦1MB */

        BLE_PRINTF("[OTA]   需擦除 %lu 个块 (每块 64KB, 共 %lu KB)...\r\n",
                   blocks_needed, blocks_needed * 64UL);

        for (unsigned long b = 0; b < blocks_needed; b++)
        {
            uint32_t blk_addr = OTA_W25Q_FW_START_ADDR + (b * W25Q_BLOCK_SIZE_64K);
            w25q128_blockErase64K(blk_addr);
            BLE_PRINTF("[OTA]   擦除进度: %lu/%lu 块 (0x%08lX)\r\n",
                       b + 1, blocks_needed, (unsigned long)blk_addr);
        }
    }
    BLE_PRINTF("[OTA]   W25Q128 擦除完成\r\n");

    /* ------ 4.3 重置三缓冲下载状态 + IPD状态机 ------ */
    /* 非透传模式: IPD状态机剥 "+IPD,len:" 前缀, 只有payload(HTTP头+固件)写入W25Q128 */
    g_bin_total_len     = 0;
    g_bin_recv_len      = 0;
    g_w25q_write_addr   = OTA_W25Q_FW_START_ADDR;
    g_http_state        = HTTP_ST_BODY;
    last_print_len      = 0;

    /* === 三缓冲初始化 (L12-27定义的全局变量) === */
    g_ota_download_flag  = 0;   /* 先关, 发送完HTTP请求再开 */
    g_ota_rx_buf         = g_ota_bufA;
    g_ota_rx_cnt         = 0;
    g_ota_bufA_full      = 0;
    g_ota_bufB_full      = 0;
    g_ota_bufC_full      = 0;
    g_ota_w25q_addrP     = OTA_W25Q_FW_START_ADDR;
    g_ota_total_recv     = 0;
    g_ota_overflow_cnt   = 0;
    memset(g_ota_bufA, 0, OTA_DBUF_SIZE);
    memset(g_ota_bufB, 0, OTA_DBUF_SIZE);
    memset(g_ota_bufC, 0, OTA_DBUF_SIZE);

    /* === IPD状态机初始化 (非透传必须重置, 避免上一包残留状态导致首包丢) === */
    g_ipd_state       = IPD_ST_IDLE;
    g_ipd_expect_len  = 0;
    g_ipd_recv_len    = 0;
    g_ipd_skip_cnt    = 0;
    /* === IPD统计重置 === */
    g_st_ipd_pkt_ok   = 0;
    g_st_ipd_pkt_badpre = 0;
    g_st_ipd_pkt_badlen = 0;
    g_st_ipd_pkt_empty  = 0;
    g_st_ipd_idle_drop  = 0;
    g_st_ipd_max_len    = 0;
    g_st_ipd_min_len    = 0xFFFFFFFFU;

    /* 清空接收缓冲 */
    USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
    u3_count = 0;
    memset(recv_buff, 0, RX_BUFF_SIZE);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    /* ------ 4.4 ESP8266 非透传模式 + TCP 连接服务器 ------ */
    BLE_PRINTF("[OTA] 4.4 连接 %s:%d (非透传模式+CIPMODE=0)...\r\n", bin_host, bin_port);
    {
        /* 保持非透传模式 (AT+CIPMODE=0): 每个TCP包带 "+IPD,len:" 前缀
           优势: ESP8266明确标注包边界, 不会出现透传模式下字节错位/丢失问题 */
        ESP8266_Send_AT("AT+CIPMODE=0\r\n");
        ESP8266_Recv_ACK("OK", 3000);

        char tcpcmd[128];
        if (bin_ssl)
        {
            BLE_PRINTF("[OTA]   尝试开启 SSL (CIPSSL=1)\r\n");
            ESP8266_Send_AT("AT+CIPSSL=1\r\n");
            ESP8266_Recv_ACK("OK", 3000);
            sprintf(tcpcmd, "AT+CIPSTART=\"SSL\",\"%s\",%d\r\n", bin_host, bin_port);
        }
        else
        {
            sprintf(tcpcmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", bin_host, bin_port);
        }
        ESP8266_Send_AT(tcpcmd);
        ret = ESP8266_Recv_ACK("CONNECT", 15000);
        BLE_PRINTF("[OTA]   连接结果: %s\r\n", (ret == 0) ? "成功" : "失败");
        if (ret != 0)
        {
            BLE_PRINTF("[OTA] 错误: TCP 连接失败, 终止下载\r\n");
            return -1;
        }
    }

    /* ------ 4.5 发送 HTTP GET 请求 (非透传: AT+CIPSEND=指定长度) ------ */
    BLE_PRINTF("[OTA] 4.5 发送 HTTP GET 请求 (非透传 CIPSEND=len)...\r\n");
    {
        char http_req[512];
        int  req_len;

        req_len = sprintf(http_req,
                          "GET %s HTTP/1.1\r\n"
                          "Host: %s\r\n"
                          "User-Agent: STM32F4-ESP8266-OTA\r\n"
                          "Accept: application/octet-stream, */*\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          bin_path, bin_host);

        BLE_PRINTF("[OTA]   HTTP 请求长度=%d 字节\r\n", req_len);
        {
            int print_len = req_len;
            int off = 0;
            int seg = 0;
            while (off < print_len)
            {
                char seg_buf[201];
                int chunk = print_len - off;
                if (chunk > 200) chunk = 200;
                memcpy(seg_buf, http_req + off, chunk);
                seg_buf[chunk] = '\0';
                BLE_PRINTF("[REQ%d] %s\r\n", seg, seg_buf);
                off += chunk;
                seg++;
                delay_ms(30);
            }
        }

        /* 非透传模式: AT+CIPSEND=xxx 指定长度发送, 收到 '>' 后写HTTP请求 */
        {
            char cipcmd[32];
            sprintf(cipcmd, "AT+CIPSEND=%d\r\n", req_len);
            ESP8266_Send_AT(cipcmd);
            ret = ESP8266_Recv_ACK(">", 5000);
            if (ret != 0)
            {
                BLE_PRINTF("[OTA] 错误: CIPSEND=%d 未收到 '>' 提示符\r\n", req_len);
                ESP8266_Send_AT("AT+CIPCLOSE\r\n");
                return -1;
            }
        }
        ESP8266_Send_AT(http_req);
        /* 等待 "SEND OK" (非透传下ESP8266会在数据发送完毕返回SEND OK)
           不能等太久, 服务器可能在SEND OK返回前就开始回包了 */
        (void)ESP8266_Recv_ACK("SEND OK", 2000);

        /* 发送后: 关中断清空接收缓冲 + 启动下载模式(ISR进入IPD状态机) */
        USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
        u3_count = 0;
        memset(recv_buff, 0, RX_BUFF_SIZE);
        /* ===== 关中断期间切换到三缓冲下载模式 + IPD状态机重置 ===== */
        g_ota_download_flag  = 1;   /* 现在开始, ISR走IPD剥前缀 -> 三缓冲分支 */
        g_ota_rx_buf         = g_ota_bufA;
        g_ota_rx_cnt         = 0;
        g_ota_bufA_full      = 0;
        g_ota_bufB_full      = 0;
        g_ota_bufC_full      = 0;
        g_ota_total_recv     = 0;
        g_ota_overflow_cnt   = 0;
        g_ipd_state          = IPD_ST_IDLE;
        g_ipd_expect_len     = 0;
        g_ipd_recv_len       = 0;
        g_ipd_skip_cnt       = 0;
        /* 下载前二次清零IPD统计 (隔离SEND OK/CONNECT回显的干扰计数) */
        g_st_ipd_pkt_ok      = 0;
        g_st_ipd_pkt_badpre  = 0;
        g_st_ipd_pkt_badlen  = 0;
        g_st_ipd_pkt_empty   = 0;
        g_st_ipd_idle_drop   = 0;
        g_st_ipd_max_len     = 0;
        g_st_ipd_min_len     = 0xFFFFFFFFU;
        USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
        BLE_PRINTF("[OTA]   HTTP 请求已发送 (非透传), 开始IPD解析接收 (三4KB缓冲)...\r\n");
    }

    /* ------ 4.6 三缓冲接收 + 写入 W25Q128 (静默模式: 下载期间无任何调试输出) ------
     *   原理: 4KB缓冲 @115200 baud = 4096*87us = 356ms 填一个
     *         4KB Flash写入(分16次页编程+WaitBusy) ≈ 50ms (已加速SPI)
     *         三缓冲提供 2×356ms = 712ms 绝对余量, ISR永远不会被迫原地覆盖
     *   关键: 每个缓冲写完4KB后, 关中断原子清 *_full 标志, 防止ISR在写完中途切回来覆盖
     */
    BLE_PRINTF("[OTA] 4.6 开始接收 (三4KB缓冲静默模式, 下载完成后打印结果)...\r\n");
    {
        int idle_cnt = 0;
        int idle_timeout_ticks = 1500;  /* 3000ms 空闲超时 (1500 tick × 2ms) */
        unsigned long last_total_recv = 0;

        while (1)
        {
            /* ===== 检查缓冲A是否满, 写W25Q128 (分16x256B页编程, 自动跨页) ===== */
            if (g_ota_bufA_full)
            {
                uint16_t offset = 0;
                while (offset < OTA_DBUF_SIZE)
                {
                    uint32_t addr = g_ota_w25q_addrP + offset;
                    uint16_t page_remain = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
                    uint16_t chunk = (OTA_DBUF_SIZE - offset < page_remain) ?
                                      (OTA_DBUF_SIZE - offset) : page_remain;
                    w25q128_WritePageData(addr, g_ota_bufA + offset, chunk);
                    offset += chunk;
                }
                g_ota_w25q_addrP += OTA_DBUF_SIZE;
                /* ★原子操作: 关中断清full标志, 避免SPI写入期间ISR切回覆盖A ★ */
                USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
                g_ota_bufA_full = 0;
                USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
            }
            /* ===== 检查缓冲B是否满, 写W25Q128 ===== */
            if (g_ota_bufB_full)
            {
                uint16_t offset = 0;
                while (offset < OTA_DBUF_SIZE)
                {
                    uint32_t addr = g_ota_w25q_addrP + offset;
                    uint16_t page_remain = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
                    uint16_t chunk = (OTA_DBUF_SIZE - offset < page_remain) ?
                                      (OTA_DBUF_SIZE - offset) : page_remain;
                    w25q128_WritePageData(addr, g_ota_bufB + offset, chunk);
                    offset += chunk;
                }
                g_ota_w25q_addrP += OTA_DBUF_SIZE;
                /* ★原子清标志★ */
                USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
                g_ota_bufB_full = 0;
                USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
            }
            /* ===== 检查缓冲C是否满, 写W25Q128 ===== */
            if (g_ota_bufC_full)
            {
                uint16_t offset = 0;
                while (offset < OTA_DBUF_SIZE)
                {
                    uint32_t addr = g_ota_w25q_addrP + offset;
                    uint16_t page_remain = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
                    uint16_t chunk = (OTA_DBUF_SIZE - offset < page_remain) ?
                                      (OTA_DBUF_SIZE - offset) : page_remain;
                    w25q128_WritePageData(addr, g_ota_bufC + offset, chunk);
                    offset += chunk;
                }
                g_ota_w25q_addrP += OTA_DBUF_SIZE;
                /* ★原子清标志★ */
                USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
                g_ota_bufC_full = 0;
                USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
            }

            /* ===== 空闲检测 ===== */
            if (g_ota_total_recv != last_total_recv)
            {
                idle_cnt = 0;
                last_total_recv = g_ota_total_recv;
            }
            else
            {
                idle_cnt++;
            }

            /* 空闲3秒超时, 下载结束 */
            if (idle_cnt > idle_timeout_ticks)
                break;

            delay_ms(2);
        }  /* while (1) */

        /* ===== 退出下载模式: 先关flag, 再刷入未满4KB的尾部 ===== */
        USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
        g_ota_download_flag = 0;   /* ISR现在停止写三缓冲, 恢复走普通recv_buff分支 */
        USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

        /* 刷入尾部不满4KB的字节 (按页分批写入) */
        if (g_ota_rx_cnt > 0)
        {
            const uint8_t *tail_buf;
            if (g_ota_rx_buf == g_ota_bufA)      tail_buf = g_ota_bufA;
            else if (g_ota_rx_buf == g_ota_bufB) tail_buf = g_ota_bufB;
            else                                  tail_buf = g_ota_bufC;
            uint16_t tail_len = g_ota_rx_cnt;
            uint16_t offset = 0;
            while (offset < tail_len)
            {
                uint32_t addr = g_ota_w25q_addrP + offset;
                uint16_t page_remain = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
                uint16_t chunk = (tail_len - offset < page_remain) ?
                                  (tail_len - offset) : page_remain;
                w25q128_WritePageData(addr, (uint8_t *)tail_buf + offset, chunk);
                offset += chunk;
            }
            g_ota_w25q_addrP += tail_len;
            g_ota_rx_cnt = 0;
        }

        /* 同步: g_bin_recv_len 供后续步骤打印, g_w25q_write_addr 供其他函数使用 */
        g_bin_recv_len    = g_ota_total_recv;
        g_w25q_write_addr = g_ota_w25q_addrP;

        /* ========== 下载后一次性打印 IPD + 缓冲统计 (不影响下载, 下载后才执行) ========== */
        {
            uint32_t min_len_show = (g_st_ipd_min_len == 0xFFFFFFFFU) ? 0 : g_st_ipd_min_len;
            unsigned long ipd_total_payload = 0;
            /* 估算: 平均包长 × 成功包数 ≈ total_recv (只做参考打印) */
            ipd_total_payload = g_ota_total_recv;

            BLE_PRINTF("\r\n[OTA] ======= IPD解析统计 (非透传模式) =======\r\n");
            BLE_PRINTF("[OTA]   +IPD包解析成功 : %lu 个\r\n", (unsigned long)g_st_ipd_pkt_ok);
            BLE_PRINTF("[OTA]   IPD前缀不匹配  : %lu 次 (异常包开头/回显混入)\r\n", (unsigned long)g_st_ipd_pkt_badpre);
            BLE_PRINTF("[OTA]   IPD长度异常    : %lu 次 (长度字段非数字非:)\r\n", (unsigned long)g_st_ipd_pkt_badlen);
            BLE_PRINTF("[OTA]   IPD空包(len=0) : %lu 次\r\n", (unsigned long)g_st_ipd_pkt_empty);
            BLE_PRINTF("[OTA]   IDLE丢弃字节   : %lu 字节 (SEND OK/状态回显等)\r\n", (unsigned long)g_st_ipd_idle_drop);
            BLE_PRINTF("[OTA]   单个IPD包最小  : %lu 字节\r\n", (unsigned long)min_len_show);
            BLE_PRINTF("[OTA]   单个IPD包最大  : %lu 字节 (典型MSS≈1460)\r\n", (unsigned long)g_st_ipd_max_len);
            BLE_PRINTF("[OTA]   三缓冲溢出次数: %lu 次 (应为0, >0说明SPI写入过慢)\r\n", (unsigned long)g_ota_overflow_cnt);
            BLE_PRINTF("[OTA]   IPD数据入缓冲 : %lu 字节 (已剥+IPD前缀)\r\n", (unsigned long)ipd_total_payload);
            BLE_PRINTF("[OTA]   下载结束时IPD状态机末状态 = %d (0=IDLE正常)\r\n", (int)g_ipd_state);
            BLE_PRINTF("[OTA] ========================================\r\n\n");
        }
    }

    /* ------ 4.7 关闭 TCP 连接 (非透传模式: 无需+++退出透传) ------ */
    BLE_PRINTF("[OTA] 4.7 关闭 TCP 连接...\r\n");
    /* 先确保退出下载模式 (ISR停止IPD解析, 恢复普通recv_buff接收) 才能正确接收CIPCLOSE的OK回显 */
    USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
    g_ota_download_flag = 0;
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    /* 关闭 TCP 连接 (Connection: close 服务器会主动断开, 但主动关更稳妥) */
    ESP8266_Send_AT("AT+CIPCLOSE\r\n");
    (void)ESP8266_Recv_ACK("OK", 3000);
    /* 确保非透传模式保持 (给后续其他AT调用用) */
    ESP8266_Send_AT("AT+CIPMODE=0\r\n");
    (void)ESP8266_Recv_ACK("OK", 2000);

    /* ------ 4.8 从W25Q128解析HTTP头, 提取固件 ------ */
    BLE_PRINTF("[OTA] 4.8 解析下载结果...\r\n");
    BLE_PRINTF("[OTA]   W25Q128 总写入 = %lu 字节\r\n", g_bin_recv_len);

    /* 不管下载是否成功, 打印 W25Q128 数据供对比实际 bin */
    {
        uint8_t dump_buf[16];
        uint32_t dump_addr;
        uint32_t total = g_bin_recv_len;

        BLE_PRINTF("[OTA] === W25Q128 数据转储 (%lu 字节) ===\r\n", (unsigned long)total);

        /* 前 512 字节 (32行) */
        BLE_PRINTF("[OTA] --- 前 512 字节 ---\r\n");
        for (dump_addr = 0; dump_addr < 512 && dump_addr < total; dump_addr += 16)
        {
            uint16_t n = (total - dump_addr < 16) ? (uint16_t)(total - dump_addr) : 16;
            uint8_t asc[17];
            uint16_t i;
            w25q128_readdata(OTA_W25Q_FW_START_ADDR + dump_addr, dump_buf, n);
            for (i = 0; i < 16; i++)
                asc[i] = (i < n && dump_buf[i] >= 0x20 && dump_buf[i] < 0x7F) ? dump_buf[i] : '.';
            asc[16] = '\0';
            BLE_PRINTF("[OTA] %05lX: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X |%s|\r\n",
                       (unsigned long)dump_addr,
                       dump_buf[0],dump_buf[1],dump_buf[2],dump_buf[3],dump_buf[4],dump_buf[5],dump_buf[6],dump_buf[7],
                       dump_buf[8],dump_buf[9],dump_buf[10],dump_buf[11],dump_buf[12],dump_buf[13],dump_buf[14],dump_buf[15],
                       asc);
            delay_ms(80);
        }

        /* 每 2KB 采样 16 字节 */
        if (total > 1024)
        {
            BLE_PRINTF("[OTA] --- 每 2KB 采样 ---\r\n");
            for (dump_addr = 2048; dump_addr + 16 <= total; dump_addr += 2048)
            {
                w25q128_readdata(OTA_W25Q_FW_START_ADDR + dump_addr, dump_buf, 16);
                BLE_PRINTF("[OTA] %05lX: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                           (unsigned long)dump_addr,
                           dump_buf[0],dump_buf[1],dump_buf[2],dump_buf[3],dump_buf[4],dump_buf[5],dump_buf[6],dump_buf[7],
                           dump_buf[8],dump_buf[9],dump_buf[10],dump_buf[11],dump_buf[12],dump_buf[13],dump_buf[14],dump_buf[15]);
                delay_ms(80);
            }
        }

        /* 最后 512 字节 */
        if (total > 512)
        {
            BLE_PRINTF("[OTA] --- 最后 512 字节 ---\r\n");
            for (dump_addr = (total > 512) ? (total - 512) : 0; dump_addr + 16 <= total; dump_addr += 16)
            {
                w25q128_readdata(OTA_W25Q_FW_START_ADDR + dump_addr, dump_buf, 16);
                BLE_PRINTF("[OTA] %05lX: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                           (unsigned long)dump_addr,
                           dump_buf[0],dump_buf[1],dump_buf[2],dump_buf[3],dump_buf[4],dump_buf[5],dump_buf[6],dump_buf[7],
                           dump_buf[8],dump_buf[9],dump_buf[10],dump_buf[11],dump_buf[12],dump_buf[13],dump_buf[14],dump_buf[15]);
                delay_ms(80);
            }
        }

        BLE_PRINTF("[OTA] === 转储结束 ===\r\n");
    }

    /* 从W25Q128读出前1KB, 搜索HTTP头 */
    {
        static uint8_t head_buf[1024];
        char *body_ptr;
        char *cl_ptr;
        char *status_line_end;
        unsigned long content_length = 0;
        int http_status = 0;
        uint16_t read_len = (g_bin_recv_len > 1024) ? 1024 : (uint16_t)g_bin_recv_len;

        w25q128_readdata(OTA_W25Q_FW_START_ADDR, head_buf, read_len);
        head_buf[read_len - 1] = '\0';  /* 确保字符串终止 */

        /* ---- 先找HTTP首行, 提取状态码 ---- */
        if (strncmp((char *)head_buf, "HTTP/", 5) == 0)
        {
            sscanf((char *)head_buf, "HTTP/1.%*c %d", &http_status);
            status_line_end = strstr((char *)head_buf, "\r\n");
            if (status_line_end != NULL)
            {
                char tmp = *status_line_end;
                *status_line_end = '\0';
                BLE_PRINTF("[OTA]   状态行: %s\r\n", (char *)head_buf);   /* 如 HTTP/1.1 200 OK */
                *status_line_end = tmp;
            }
            BLE_PRINTF("[OTA]   HTTP 状态码 = %d (%s)\r\n", http_status,
                       (http_status == 200) ? "OK" : "非200,下载失败!");
        }
        else
        {
            BLE_PRINTF("[OTA]   警告: 未发现HTTP/开头! 首8字节: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                       head_buf[0], head_buf[1], head_buf[2], head_buf[3],
                       head_buf[4], head_buf[5], head_buf[6], head_buf[7]);
        }

        /* 搜索 \r\n\r\n 找到 body 起始 */
        body_ptr = strstr((char *)head_buf, "\r\n\r\n");
        if (body_ptr == NULL)
        {
            BLE_PRINTF("[OTA] 错误: 未找到 HTTP 头结束符 (\\r\\n\\r\\n)\r\n");
            BLE_PRINTF("[OTA]   前64字节: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                       head_buf[0], head_buf[1], head_buf[2], head_buf[3],
                       head_buf[4], head_buf[5], head_buf[6], head_buf[7]);
            return -1;
        }

        g_fw_offset = (uint32_t)(body_ptr - (char *)head_buf) + 4;  /* +4 跳过 \r\n\r\n */

        /* 搜索 Content-Length */
        cl_ptr = strstr((char *)head_buf, "Content-Length:");
        if (cl_ptr != NULL)
        {
            sscanf(cl_ptr, "Content-Length: %lu", &content_length);
            BLE_PRINTF("[OTA]   Content-Length: 命中 ✓ = %lu 字节\r\n", content_length);
        }
        else
        {
            BLE_PRINTF("[OTA]   Content-Length: 缺失 ✗ (使用实际接收量判断)\r\n");
        }
        g_bin_total_len = content_length;

        BLE_PRINTF("[OTA]   HTTP头长度 = %lu 字节\r\n", (unsigned long)g_fw_offset);
        BLE_PRINTF("[OTA]   Content-Length = %lu 字节\r\n", content_length);
        BLE_PRINTF("[OTA]   固件大小 = %lu 字节 (总接收 - HTTP头 = %lu - %lu)\r\n",
                   (unsigned long)(g_bin_recv_len - g_fw_offset),
                   (unsigned long)g_bin_recv_len, (unsigned long)g_fw_offset);
        if (content_length > 0)
        {
            long diff = (long)((g_bin_recv_len - g_fw_offset) - content_length);
 
					 BLE_PRINTF("[OTA]   实际 vs Content-Length 差异 = %ld 字节 (%s)\r\n",diff, (diff == 0) ? "完全匹配" : (diff > 0 ? "数据偏多" : "数据偏少,缺字节!"));
        }

        /* 打印固件前 16 字节(向量表) */
        if (g_bin_recv_len - g_fw_offset >= 16)
        {
            uint8_t vect[16];
            w25q128_readdata(OTA_W25Q_FW_START_ADDR + g_fw_offset, vect, 16);
            BLE_PRINTF("[OTA]   固件前16字节(向量表):\r\n");
            BLE_PRINTF("[OTA]   %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                       vect[0], vect[1], vect[2], vect[3],
                       vect[4], vect[5], vect[6], vect[7]);
            BLE_PRINTF("[OTA]   %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                       vect[8],  vect[9],  vect[10], vect[11],
                       vect[12], vect[13], vect[14], vect[15]);
        }
    }

    /* 大小一致性检查 */
    if (g_bin_total_len > 0 && (g_bin_recv_len - g_fw_offset) != g_bin_total_len)
    {
        BLE_PRINTF("[OTA] 错误: 固件大小不匹配! 期望=%lu 实际=%lu\r\n",
                   g_bin_total_len, (unsigned long)(g_bin_recv_len - g_fw_offset));
        return -1;
    }

    if (g_bin_recv_len - g_fw_offset < 1024)
    {
        BLE_PRINTF("[OTA] 错误: 固件过小 (%lu 字节)\r\n",
                   (unsigned long)(g_bin_recv_len - g_fw_offset));
        return -1;
    }

    BLE_PRINTF("[OTA] ======= 固件下载到 W25Q128: 成功! =======\r\n\r\n");
    return 0;
}


   
int OTA_HTTP_Test(void)
{
    static char meta_resp[OTA_META_RESP_SIZE];
    static char bin_resp[OTA_BIN_RESP_SIZE];
    ota_meta_t meta;
    int  ret;

    BLE_PRINTF("\r\n======== [OTA] 固件查询与下载 ========\r\n");

    BLE_PRINTF("[OTA] 步骤1: 初始化 ESP8266\r\n");
    if (_OTA_InitESP8266() != 0)
    {
        BLE_PRINTF("[OTA] 错误: ESP8266 初始化失败, 终止 OTA\r\n");
        return -1;
    }

		////https://apis.bemfa.com/vb/api/v1/firmwareVersion?openID=3f2d5feab40ea3844d23321f165ab2db&topic=ota&deviceType=3
    BLE_PRINTF("[OTA] 步骤2: HTTP 查询固件版本 (bemfa API)\r\n");
    BLE_PRINTF("[OTA]   URL: http://%s%s\r\n", OTA_META_HOST, OTA_META_PATH);
    ret = _OTA_HTTP_Get(OTA_META_HOST, OTA_META_PORT, OTA_META_PATH,
                         meta_resp, OTA_META_RESP_SIZE, 5000, 0);
    if (ret != 0)
    {
        BLE_PRINTF("[OTA] 错误: 元数据请求失败\r\n");
        return -1;
    }

    BLE_PRINTF("[OTA]   完整响应 (%d 字节), 分段打印:\r\n", (int)strlen(meta_resp));
    meta_resp[OTA_META_RESP_SIZE - 1] = '\0';
    {
        int total_len = strlen(meta_resp);
        int offset = 0;
        int seg = 0;
        while (offset < total_len)
        {
            char seg_buf[201];
            int chunk = total_len - offset;
            if (chunk > 200) chunk = 200;
            memcpy(seg_buf, meta_resp + offset, chunk);
            seg_buf[chunk] = '\0';
            BLE_PRINTF("[seg%d] %s\r\n", seg, seg_buf);
            offset += chunk;
            seg++;
            delay_ms(50);
        }
        BLE_PRINTF("[OTA]   打印完成, 共 %d 段\r\n", seg);
    }

    BLE_PRINTF("[OTA] 步骤3: 解析 JSON\r\n");
    {
        char *json_body = strchr(meta_resp, '{');
        if (json_body == NULL)
        {
            BLE_PRINTF("[OTA] 错误: 未找到 JSON 起始符 '{'\r\n");
            return -1;
        }

        BLE_PRINTF("[OTA]   JSON 起始偏移: %d 字节\r\n",
                    (int)(json_body - meta_resp));

        if (_OTA_ParseJsonMetadata(json_body, &meta) != 0)
        {
            BLE_PRINTF("[OTA] 错误: JSON 解析失败 (code=%d url=%s)\r\n", meta.code, meta.url);
            return -1;
        }
    }

    BLE_PRINTF("[OTA]   code=%d msg=%s\r\n", meta.code, meta.msg);
    BLE_PRINTF("[OTA]   version=%d size=%lu unix=%llu\r\n", meta.version,
                (unsigned long)meta.size, meta.unix);
    BLE_PRINTF("[OTA]   tag=%s\r\n", meta.tag);
    BLE_PRINTF("[OTA]   url=%s\r\n", meta.url);

		
    /* ---- 步骤4: 固件下载 (从 meta.url 写入 W25Q128) ---- */
    BLE_PRINTF("[OTA] 步骤4: 下载固件 (从元数据URL写入W25Q128)\r\n");
    ret = codeDownload(meta.url);
    if (ret != 0)
    {
        BLE_PRINTF("[OTA] 错误: 固件下载到 W25Q128 失败, OTA 终止\r\n");
        return -1;
    }

    /* ---- 步骤5: 元数据 size 与实际下载字节数交叉校验 ---- */
    BLE_PRINTF("[OTA] 步骤5: 校验元数据 vs 实际下载\r\n");
    BLE_PRINTF("[OTA]   元数据声明 size=%lu 字节\r\n", (unsigned long)meta.size);
    BLE_PRINTF("[OTA]   HTTP Content-Length=%lu 字节\r\n", g_bin_total_len);
    BLE_PRINTF("[OTA]   W25Q128 总写入=%lu 字节 (含HTTP头)\r\n", g_bin_recv_len);
    BLE_PRINTF("[OTA]   固件实际大小=%lu 字节 (去掉HTTP头%lu字节)\r\n",
               (unsigned long)(g_bin_recv_len - g_fw_offset),
               (unsigned long)g_fw_offset);

    if (meta.size > 0 && (g_bin_recv_len - g_fw_offset) != meta.size)
    {
        BLE_PRINTF("[OTA]   警告: 元数据size与固件实际大小不一致 (差=%ld字节)\r\n",
                   (long)((g_bin_recv_len - g_fw_offset) - meta.size));
    }
    else
    {
        BLE_PRINTF("[OTA]   校验通过: 大小匹配\r\n");
    }

    BLE_PRINTF("\r\n[OTA] ===== OTA HTTP Test (查询+下载) 全部成功! =====\r\n");
    BLE_PRINTF("[OTA]   固件版本号(tag+version): %s_V%d\r\n", meta.tag, meta.version);
    BLE_PRINTF("[OTA]   固件存储: W25Q128 @ 0x00000000+%lu, 大小=%lu 字节\r\n",
               (unsigned long)g_fw_offset,
               (unsigned long)(g_bin_recv_len - g_fw_offset));
    BLE_PRINTF("[OTA]   开始搬运: 从W25Q128写入内部Flash并跳转\r\n");

    /* 步骤6: 搬运 W25Q128 固件 -> 内部 Flash, 校验, 设置跳转标志, 复位 */
    ret = OTA_FlashAndJump();
    if (ret != 0)
    {
        BLE_PRINTF("[OTA] 错误: 搬运到内部Flash失败, OTA 终止\r\n");
        return -1;
    }
    return 0;
}

/**
 * @brief  OTA 搬运与跳转: 从 W25Q128 读取固件写入内部 Flash, 校验后复位跳转
 *
 * 流程:
 *   1. 判断当前分区(BKP1R), 决定写入到对侧分区 (A->B 或 B->A)
 *   2. 检测 W25Q128 数据是否带 HTTP 头, 计算固件偏移
 *   3. 解锁内部 Flash, 擦除目标分区3个扇区 (128KB x 3 = 384KB)
 *   4. 从 W25Q128 读 4KB/块, 按半字(16-bit)编程到内部 Flash
 *   5. memcmp 读回校验
 *   6. 设置 BKP0R 跳转标志, 触发系统复位 (不会返回)
 *
 * 返回: 0=成功(实际不会返回,会复位) -1=失败
 */
int OTA_FlashAndJump(void)
{
    uint32_t last_boot, target_addr, target_flag;
    uint32_t fw_offset, fw_size;
    uint8_t  head_buf[5];
    uint8_t  w25q_buf[4096];
    uint8_t  verify_buf[4096];
    uint32_t written, verified;
    int      i, fail;

    /* ---- 1. 判断目标分区: 优先使用BKP0R中的OTA_UPGRADE_A/B标志(APP长按按键写入) ---- */
    uint32_t ota_req = RTC->BKP0R;
    if (ota_req == OTA_UPGRADE_A)
    {
        /* APP长按: 当前运行B, 明确请求升级到A分区 */
        target_addr = OTA_APP_A_ADDRESS;
        target_flag = OTA_JUMP_FLAG_TO_A;
        BLE_PRINTF("[OTA-FLASH] BKP0R=OTA_UPGRADE_A, 将写入A分区 (0x%08X)\r\n", target_addr);
    }
    else if (ota_req == OTA_UPGRADE_B)
    {
        /* APP长按: 当前运行A, 明确请求升级到B分区 */
        target_addr = OTA_APP_B_ADDRESS;
        target_flag = OTA_JUMP_FLAG_TO_B;
        BLE_PRINTF("[OTA-FLASH] BKP0R=OTA_UPGRADE_B, 将写入B分区 (0x%08X)\r\n", target_addr);
    }
    else
    {
        /* 无明确OTA_UPGRADE标志时, 回退使用BKP1R(上次运行记录)判断 */
        last_boot = RTC->BKP1R;
        if (last_boot == OTA_LAST_BOOT_TO_B)
        {
            /* 上次运行B, 这次写入到A */
            target_addr = OTA_APP_A_ADDRESS;
            target_flag = OTA_JUMP_FLAG_TO_A;
            BLE_PRINTF("[OTA-FLASH] 上次运行B(BKP1R), 将写入A分区 (0x%08X)\r\n", target_addr);
        }
        else
        {
            /* 默认或上次是A, 这次写入到B */
            target_addr = OTA_APP_B_ADDRESS;
            target_flag = OTA_JUMP_FLAG_TO_B;
            BLE_PRINTF("[OTA-FLASH] 上次运行A(BKP1R)或默认, 将写入B分区 (0x%08X)\r\n", target_addr);
        }
    }

    /* ---- 2. 检测 W25Q128 数据格式(带HTTP头 or 纯bin) ---- */
    w25q128_readdata(OTA_W25Q_FW_START_ADDR, head_buf, 5);
    if (memcmp(head_buf, "HTTP/", 5) == 0)
    {
        /* 带HTTP响应头: 使用 codeDownload 已解析出的 g_fw_offset */
        fw_offset = g_fw_offset;
        BLE_PRINTF("[OTA-FLASH] 检测到HTTP头, 固件偏移=%lu\r\n", (unsigned long)fw_offset);
    }
    else
    {
        /* 纯bin: 从0开始 */
        fw_offset = 0;
        BLE_PRINTF("[OTA-FLASH] 无HTTP头, 纯bin文件\r\n");
    }

    if (g_bin_recv_len <= fw_offset)
    {
        BLE_PRINTF("[OTA-FLASH] 错误: 接收数据(%lu) <= HTTP头(%lu)\r\n",
                   (unsigned long)g_bin_recv_len, (unsigned long)fw_offset);
        return -1;
    }
    fw_size = g_bin_recv_len - fw_offset;
    BLE_PRINTF("[OTA-FLASH] 固件大小=%lu 字节, 目标=0x%08X\r\n",
               (unsigned long)fw_size, target_addr);

    /* ---- 3. 解锁内部 Flash, 擦除目标分区3个扇区 ---- */
    BLE_PRINTF("[OTA-FLASH] 解锁内部Flash, 开始擦除...\r\n");
    FLASH_Unlock();

    /* 擦除扇区 (VoltageRange_3 = 2.7-3.6V, 我们VDD=3.3V) */
    if (target_addr == OTA_APP_A_ADDRESS)
    {
        /* APP A: Sector 6,7,8 (0x08040000-0x0809FFFF, 384KB) */
        BLE_PRINTF("[OTA-FLASH] 擦除 Sector 6...\r\n");
        if (FLASH_EraseSector(FLASH_Sector_6, VoltageRange_3) != FLASH_COMPLETE) goto erase_fail;
        BLE_PRINTF("[OTA-FLASH] 擦除 Sector 7...\r\n");
        if (FLASH_EraseSector(FLASH_Sector_7, VoltageRange_3) != FLASH_COMPLETE) goto erase_fail;
        BLE_PRINTF("[OTA-FLASH] 擦除 Sector 8...\r\n");
        if (FLASH_EraseSector(FLASH_Sector_8, VoltageRange_3) != FLASH_COMPLETE) goto erase_fail;
    }
    else
    {
        /* APP B: Sector 9,10,11 (0x080A0000-0x080FFFFF, 384KB) */
        BLE_PRINTF("[OTA-FLASH] 擦除 Sector 9...\r\n");
        if (FLASH_EraseSector(FLASH_Sector_9, VoltageRange_3) != FLASH_COMPLETE) goto erase_fail;
        BLE_PRINTF("[OTA-FLASH] 擦除 Sector 10...\r\n");
        if (FLASH_EraseSector(FLASH_Sector_10, VoltageRange_3) != FLASH_COMPLETE) goto erase_fail;
        BLE_PRINTF("[OTA-FLASH] 擦除 Sector 11...\r\n");
        if (FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3) != FLASH_COMPLETE) goto erase_fail;
    }
    BLE_PRINTF("[OTA-FLASH] 擦除完成\r\n");

    /* ---- 4. 从 W25Q128 读 4KB/块, 按半字(16-bit)编程到内部 Flash ---- */
    BLE_PRINTF("[OTA-FLASH] 开始写入内部Flash...\r\n");
    written = 0;
    while (written < fw_size)
    {
        uint32_t this_chunk = (fw_size - written > 4096) ? 4096 : (fw_size - written);
        /* 偶数对齐: 半字编程需要2字节, this_chunk为奇数时减1 */
        uint32_t prog_chunk = this_chunk & ~1U;
        w25q128_readdata(OTA_W25Q_FW_START_ADDR + fw_offset + written, w25q_buf, this_chunk);

        /* 按半字(2字节)编程, 只处理prog_chunk字节(偶数) */
        for (i = 0; (uint32_t)i < prog_chunk; i += 2)
        {
            uint16_t halfword = w25q_buf[i] | ((uint16_t)w25q_buf[i + 1] << 8);
            if (FLASH_ProgramHalfWord(target_addr + written + i, halfword) != FLASH_COMPLETE)
            {
                BLE_PRINTF("[OTA-FLASH] 错误: Flash编程失败 @0x%08X\r\n",
                           target_addr + written + i);
                FLASH_Lock();
                return -1;
            }
        }

        /* 奇数尾部1字节单独编程 */
        if (prog_chunk < this_chunk)
        {
            if (FLASH_ProgramByte(target_addr + written + prog_chunk, w25q_buf[prog_chunk]) != FLASH_COMPLETE)
            {
                BLE_PRINTF("[OTA-FLASH] 错误: 末字节编程失败 @0x%08X\r\n",
                           target_addr + written + prog_chunk);
                FLASH_Lock();
                return -1;
            }
        }

        written += this_chunk;
        if ((written % 8192) == 0)
        {
            BLE_PRINTF("[OTA-FLASH] 进度: %lu/%lu (%d%%)\r\n",
                       (unsigned long)written, (unsigned long)fw_size,
                       (int)(written * 100 / fw_size));
        }
    }
    BLE_PRINTF("[OTA-FLASH] 写入完成: %lu 字节\r\n", (unsigned long)written);

    /* ---- 5. 校验: 读回对比 memcmp ---- */
    BLE_PRINTF("[OTA-FLASH] 开始校验 (memcmp read-back)...\r\n");
    verified = 0;
    fail = 0;
    while (verified < fw_size)
    {
        uint32_t this_chunk = (fw_size - verified > 4096) ? 4096 : (fw_size - verified);
        w25q128_readdata(OTA_W25Q_FW_START_ADDR + fw_offset + verified, verify_buf, this_chunk);

        if (memcmp(verify_buf, (const void *)(target_addr + verified), this_chunk) != 0)
        {
            fail = 1;
            BLE_PRINTF("[OTA-FLASH] 错误: 校验失败 @0x%08X\r\n", target_addr + verified);
            /* 打印前16字节差异 */
            for (i = 0; i < 16 && (uint32_t)i < this_chunk; i++)
            {
                uint8_t flash_byte = ((volatile uint8_t *)(target_addr + verified))[i];
                BLE_PRINTF("[OTA-FLASH]   [%d] W25Q=%02X vs Flash=%02X %s\r\n",
                           i, verify_buf[i], flash_byte,
                           (verify_buf[i] == flash_byte) ? "OK" : "DIFF");
            }
            break;
        }
        verified += this_chunk;
    }

    if (fail)
    {
        BLE_PRINTF("[OTA-FLASH] 校验失败, OTA终止\r\n");
        FLASH_Lock();
        return -1;
    }
    BLE_PRINTF("[OTA-FLASH] 校验通过: %lu 字节匹配\r\n", (unsigned long)verified);

    /* ---- 6. 锁Flash, 设置跳转标志, 触发系统复位 ---- */
    FLASH_Lock();
    BLE_PRINTF("[OTA-FLASH] 设置 BKP0R=0x%08X, 准备复位跳转到0x%08X\r\n",
               target_flag, target_addr);
    RTC->BKP0R = target_flag;
    delay_ms(200);

    BLE_PRINTF("[OTA-FLASH] === 触发系统复位 ===\r\n");
    delay_ms(50);
    /* 系统复位: SCB->AIRCR 写入 VECTKEY=0x5FA + SYSRESETREQ=1 */
    SCB->AIRCR = ((0x5FA << 16) | (1 << 2));
    while (1);  /* 复位进行中, 不应到达 */
    return 0;

erase_fail:
    BLE_PRINTF("[OTA-FLASH] 错误: 扇区擦除失败\r\n");
    FLASH_Lock();
    return -1;
}
