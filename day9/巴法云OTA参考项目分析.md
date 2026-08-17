# 巴法云 OTA 参考项目分析

> 参考项目路径：`OTA_bemfa.com`
> 平台：STM32F103C8 + 4G 模块（HTTPD 模式）+ W25Q64 外部 Flash
> 整理目的：理解 OTA 完整流程，为当前 ESP8266 项目提供借鉴

---

## 一、项目结构

```
OTA_bemfa.com/
├── BootLoader/              引导程序（负责下载+搬运+跳转）
│   ├── Hardware/
│   │   ├── OTA.c/.h         OTA 核心逻辑（下载、校验、搬运）
│   │   ├── UART2.c          4G 模块通信串口（USART2）
│   │   ├── W25Q64.c/.h      外部 Flash 驱动（下载中转存储）
│   │   └── Key.c            按键触发升级
│   ├── System/
│   │   ├── MyFLASH.c/.h     内部 Flash 读写驱动
│   │   └── Tim2.c/Tim3.c    定时器（超时检测/计数）
│   └── User/main.c          BootLoader 主程序
├── UserApplication_v1/      用户应用程序（被升级的目标）
│   └── User/main.c          用户主程序
└── UserApplication_v1_HTTP.txt  HTTP 响应报文样本
```

---

## 二、Flash 地址划分

```
STM32F103C8 内部 Flash（共 64KB）：
┌──────────────────────────────────────┐
│ BootLoader 区: 0x08000000 - 0x08004FFF (20KB)  ← 引导程序
├──────────────────────────────────────┤
│ UserApp    区: 0x08005000 - 0x0800FBFF (43KB)  ← 用户程序
├──────────────────────────────────────┤
│ 用户数据区:   0x0800FC00 - 0x0800FFFF (1KB)   ← USER_DATA
└──────────────────────────────────────┘

W25Q64 外部 Flash（8MB，作为下载中转）：
┌──────────────────────────────────────┐
│ 中转仓库: 0x000000 起  ← bin 下载后暂存于此
└──────────────────────────────────────┘
```

地址定义（[Address.h](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/Address.h)）：
```c
#define START_ADDRESS 0x08005000  // 内部 FLASH 用户程序起始地址
#define REPO_ADDRESS  0x000000    // W25Q64 中转仓库起始地址
#define USER_DATA     0x0800FC00  // 内部 FLASH 用户数据区
```

---

## 三、OTA 完整流程总览

```
上电
 │
 ▼
BootLoader 启动（main.c）
 │
 ├─ 初始化串口/定时器/按键
 ├─ 倒计时 10 秒（等待用户操作）
 │
 ├─ 10秒内按下按键？
 │   ├─ 是 → 进入 OTA 升级流程 ──────────────────┐
 │   │                                            │
 │   │   步骤1: codeDownload()  下载 bin          │
 │   │   ┌─────────────────────────────────────┐  │
 │   │   │ 1. AT 指令配置 4G 模块为 HTTPD 模式  │  │
 │   │   │ 2. 擦除 W25Q64 中转区（11×4KB）     │  │
 │   │   │ 3. 发送 HTTP GET 请求下载 bin       │  │
 │   │   │ 4. 串口中断逐字节写入 W25Q64        │  │
 │   │   │ 5. TIM2 超时检测传输结束            │  │
 │   │   │ 6. 解析 HTTP 头（长度/CRC/偏移）     │  │
 │   │   │ 7. CRC64_ECMA 校验数据完整性        │  │
 │   │   └─────────────────────────────────────┘  │
 │   │                                            │
 │   │   步骤2: codeTransport() 搬运到内部 Flash  │
 │   │   ┌─────────────────────────────────────┐  │
 │   │   │ 1. 擦除内部 Flash 用户区（43页）     │  │
 │   │   │ 2. 逐半字从 W25Q64 读到内部 Flash   │  │
 │   │   └─────────────────────────────────────┘  │
 │   │                                            │
 │   │   步骤3: 跳转到用户程序                    │
 │   │   ┌─────────────────────────────────────┐  │
 │   │   │ 1. 关闭所有中断/外设                  │  │
 │   │   │ 2. __disable_irq()                   │  │
 │   │   │ 3. __set_MSP(栈顶地址)               │  │
 │   │   │ 4. 跳转到 Reset_Handler              │  │
 │   │   └─────────────────────────────────────┘  │
 │   │                                            │
 │   └─ 否 → 10秒后直接跳转用户程序 ─────────────┘
 │
 ▼
用户程序运行（UserApplication_v1）
  └─ NVIC_SetVectorTable 设置向量表偏移到 0x08005000
```

---

## 四、连接 bemfa 获取 bin 的代码流程（codeDownload 详解）

### 4.1 配置 4G 模块进入 HTTPD 模式

[OTA.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/OTA.c) `codeDownload()` 函数前半段：

```c
ATCmd("+++", "a", 500);                        // 退出透传模式
ATCmd("a", "ok", 500);                          // 确认退出
ATCmd("AT+E=OFF\r\n", "OK", 500);              // 关闭回显
ATCmd("AT+WKMOD=HTTPD\r\n", "OK", 500);        // 设置为 HTTPD 模式
ATCmd("AT+HTPTP=GET\r\n", "OK", 500);          // HTTP 方法 = GET
ATCmd("AT+HTPURL=/\r\n", "OK", 500);           // URL 路径 = /
ATCmd("AT+HTPSV=bin.bemfa.com,80\r\n", "OK", 500); // 服务器 = bin.bemfa.com:80
ATCmd("AT+HTPTIM=10\r\n", "OK", 500);          // 超时 = 10秒
ATCmd("AT+HTPHD=[0D][0A]\r\n", "OK", 500);     // 请求头换行
ATCmd("AT+HTPPK=OFF\r\n", "OK", 500);          // 关闭响应头回复
ATCmd("AT+S\r\n", "OK", 1000);                 // 启动任务，进入透传
Delay_s(20);                                    // 预热模块延时
```

> **注意**：这是 4G 模块的 AT 指令集。ESP8266 用的是另一套指令（`AT+CIPSTART`、`AT+CIPSEND`），但目的是一样的：**建立到 `bin.bemfa.com:80` 的 HTTP 连接**。

### 4.2 擦除 W25Q64 中转区

```c
W25Q64_Init();
for (uint8_t i = 0; i < 11; i++) {
    W25Q64_SectorErase(REPO_ADDRESS + (i << 12));  // 擦除 11 个 4KB 扇区 = 44KB
}
```

> **为什么需要外部 Flash 中转？**
> 4G 模块透传模式下数据是流式到达的，无法预知总长度。如果直接写内部 Flash，擦除期间会丢失数据。所以先存到 W25Q64，存完校验后再搬运。

### 4.3 发送下载请求 + 逐字节接收

```c
DownloadCmd("b/1BcNzVkYWQxMmU1ZjJlNDZhODlmYWNkMjEyMjViZmNlMTk=otaServer010.bin", 6000);
```

- 参数1：HTTP 请求路径（含巴法云的设备凭证 + 文件名 `otaServer010.bin`）
- 参数2：6000（60秒超时）

**下载模式的中断接收**（[UART2.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/UART2.c#L165-L209)）：

```c
void USART2_IRQHandler(void) {
    if (DownloadFlag == 1) {                    // 下载模式
        RxData = USART_ReceiveData(USART2);
        W25Q64_PageProgram(REPO_ADDRESS + addrP, &RxData, 1);  // 逐字节写入 W25Q64
        TIM_SetCounter(TIM2, 0);                // 每收到1字节，重置超时计时器
        if (addrP == 0) {
            TIM_Cmd(TIM2, ENABLE);              // 第1个字节到，开启超时检测
        }
        addrP++;
    }
}

void TIM2_IRQHandler(void) {                    // 超时中断 = 传输结束
    TIM_Cmd(TIM2, DISABLE);
    DownloadFlag = 0;                           // 退出下载模式
}
```

> **关键设计**：TIM2 作为"传输结束检测器"。每收到一个字节就重置 TIM2 计数器，如果长时间没收到新字节（超时），TIM2 中断触发，认为传输结束。这比等待固定时间更可靠。

### 4.4 解析 HTTP 响应头

下载完成后，W25Q64 里存的是完整的 HTTP 响应（头 + bin 数据）：

```c
uint8_t httpHead[400] = {0};
W25Q64_ReadData(REPO_ADDRESS, httpHead, 400);    // 读前400字节

// 1. 解析 bin 文件大小
char *index = strstr((char *)httpHead, "Content-Length");
sscanf(index + 16, "%hu", &binLen);             // 如 10576 字节

// 2. 解析 CRC64 校验码
index = strstr((char *)httpHead, "crc64ecma");
sscanf(index + 11, "%llu\r\n", &crc64);         // 如 8230015840672365630

// 3. 找 bin 数据起始偏移（HTTP 头和正文之间的空行）
index = strstr((char *)httpHead, "\r\n\r\n");
offset = (uint8_t *)index + 4 - httpHead;        // 如 offset=360

// 4. 校验长度匹配
if (fullLen - offset != binLen) return 0;         // 总长 - 头长 = bin长
```

HTTP 响应报文样本（[UserApplication_v1_HTTP.txt](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/UserApplication_v1_HTTP.txt)）：

```
HTTP/1.1 200 OK
Server: nginx/1.20.2
Content-Type: application/octet-stream
Content-Length: 10576                      ← bin 文件大小
x-cos-hash-crc64ecma: 8230015840672365630  ← CRC 校验码

<这里是 \r\n\r\n 空行>                     ← offset 指向这里之后
<bin 二进制数据 10576 字节>                ← 真正的固件
```

### 4.5 CRC64_ECMA 完整性校验

```c
uint8_t CRC64_ECMA(void) {
    unsigned long long crc_value = 0xFFFFFFFFFFFFFFFF;
    uint32_t repoAddr = REPO_ADDRESS + offset;   // 从 bin 数据起始处开始
    uint8_t data;
    while (length--) {
        W25Q64_ReadData(repoAddr++, &data, 1);   // 逐字节读
        crc_value ^= data;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc_value & 1)
                crc_value = (crc_value >> 1) ^ 0xC96C5795D7870F42;
            else
                crc_value >>= 1;
        }
    }
    crc_value ^= 0xFFFFFFFFFFFFFFFF;
    if (crc_value != crc64) return 0;            // 与服务器给的 CRC 比对
    return 1;
}
```

> **为什么需要 CRC？** 4G/WiFi 传输可能丢包或错位。CRC64 保证搬运到内部 Flash 之前数据 100% 正确，避免把损坏的固件烧进去导致变砖。

---

## 五、写入 Flash 实现升级的流程（codeTransport 详解）

### 5.1 擦除内部 Flash 用户区

```c
void codeTransport(void) {
    // 擦除 UserApp 区（43 页 × 1KB = 43KB）
    for (uint8_t i = 0; i < 43; i++) {
        MyFLASH_ErasePage(START_ADDRESS + (i << 10));  // 0x08005000 + i*1024
    }
```

> F103 每页 1KB，F407 每扇区 16KB/32KB/128KB，擦除粒度不同。

### 5.2 从 W25Q64 搬运到内部 Flash

```c
    // 逐页搬运（每页 1KB = 512 个半字）
    for (uint8_t i = 0; i < 43; i++) {
        userPageAddr = START_ADDRESS + (i << 10);           // 内部 Flash 目标页
        repoAddr = REPO_ADDRESS + (i << 10) + offset;       // W25Q64 源页（跳过HTTP头）
        for (uint16_t j = 0; j < 512; j++) {                // 每页 512 个半字
            W25Q64_ReadData(repoAddr + (j<<1), halfWord, 2); // 读 2 字节
            MyFLASH_ProgramHalfWord(userPageAddr + (j<<1),  // 写半字到内部 Flash
                halfWord[0] | halfWord[1] << 8);
        }
    }
}
```

> **为什么按半字（16bit）写？** STM32 内部 Flash 编程最小单位是 16 位（半字），不能按单字节写。所以从 W25Q64 读 2 个字节，拼成 16 位再写入。

### 5.3 跳转到用户程序

[main.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/User/main.c#L74-L99)：

```c
// 1. 关闭所有中断和外设
TIM_Cmd(TIM3, DISABLE);
TIM_ITConfig(TIM3, TIM_IT_Update, DISABLE);
TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
USART_ITConfig(USART2, USART_IT_IDLE, DISABLE);
SPI_Cmd(SPI1, DISABLE);

// 2. 关全局中断
__disable_irq();

// 3. 设置主堆栈指针 MSP = 用户程序栈顶地址
__set_MSP(*(__IO uint32_t *)START_ADDRESS);   // 读 0x08005000 处的 SP

// 4. 获取用户程序的 Reset_Handler 地址
void (*userResetHandler)(void) =
    (void (*)(void))(*(__IO uint32_t *)(START_ADDRESS + 4));  // 读 0x08005004 处的 PC

// 5. 跳转
userResetHandler();   // 执行用户程序复位函数，不再返回
```

### 5.4 用户程序的向量表设置

[UserApplication_v1/main.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/UserApplication_v1/User/main.c#L27-L28)：

```c
NVIC_SetVectorTable(NVIC_VectTab_FLASH, START_ADDRESS);  // 偏移 0x08005000
__enable_irq();  // BootLoader 关了中断，这里重新打开
```

> 这和你当前项目的 `VECT_TAB_OFFSET` 是同一个作用，只是 F103 用库函数、F407 用宏。

---

## 六、关键技术点解析

### 6.1 为什么用外部 Flash 中转？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **直接写内部 Flash** | 省一个 W25Q64 | WiFi/4G 模块透传时无法停顿等擦除，数据会丢 |
| **外部 Flash 中转**（本项目） | 先存完再校验再搬运，数据可靠 | 多一颗 W25Q64 芯片 |

> ESP8266 方案如果用 `AT+CIPRECVDATA` 分段读取，可以不依赖外部 Flash，直接按块写入内部 Flash。但需要先擦后写，管理更复杂。

### 6.2 传输结束检测（TIM2 超时法）

```
数据流:  [字节1][字节2]...[字节N]......(沉默)......
                                ↑
                         TIM2 超时 → 认为传输结束
```

每收到 1 字节重置 TIM2 → 只要数据持续到达就不会超时 → 数据断流后 TIM2 溢出中断 → 标记传输结束。

### 6.3 HTTP 响应解析三要素

| 要素 | 来源 | 作用 |
|------|------|------|
| `Content-Length` | HTTP 响应头 | 知道 bin 有多大，防止少收 |
| `crc64ecma` | HTTP 响应头 | 校验 bin 完整性，防错码 |
| `\r\n\r\n` | HTTP 协议 | 定位 bin 数据起始偏移（头和正文的分界） |

### 6.4 跳转前的"善后工作"

```c
// 必须关的东西：
TIM_Cmd(TIM3, DISABLE);           // 关定时器
TIM_ITConfig(..., DISABLE);       // 关定时器中断
USART_ITConfig(..., DISABLE);     // 关串口中断
SPI_Cmd(SPI1, DISABLE);          // 关 SPI（W25Q64 用）
__disable_irq();                  // 关全局中断
```

> 跳转前如果不清这些中断，残留的中断会在跳转后触发，但用户程序的向量表还没就位，直接 HardFault。

---

## 七、与当前 ESP8266 项目的对比

| 对比项 | 参考项目（4G 模块） | 你的项目（ESP8266） |
|--------|--------------------|--------------------|
| 通信模块 | 4G 模块（HTTPD AT 指令） | ESP8266 AT 固件 |
| HTTP 方式 | 模块内置 HTTPD，AT 指令直接下载 | 需手动拼 HTTP GET 报文 + TCP 发送 |
| 中转存储 | W25Q64 外部 Flash | 可省略，用 RAM 缓冲区分段写 |
| 传输检测 | TIM2 超时法 | ESP8266 `+IPD,n:数据` 自带长度 |
| 校验方式 | CRC64_ECMA | 可简化为按长度校验（或加 CRC16） |
| Flash 写入 | 半字编程（F103） | 字编程（F407 可 32bit） |
| 分区方式 | BootLoader(20K) + App(43K) | Bootloader(256K) + A(384K) + B(384K) |

### 可借鉴的核心思路

1. **先下载到缓冲区，校验后再写 Flash** —— 数据可靠性第一
2. **HTTP 响应头解析**（Content-Length + 空行定位）—— 通用技巧
3. **跳转前关闭所有中断/外设** —— 防止 HardFault
4. **用户程序必须设向量表偏移** —— 中断才能正确响应
5. **CRC 校验** —— 防止烧入损坏固件

### ESP8266 版本的适配要点

参考项目的下载命令：
```c
// 4G 模块版（模块内置 HTTP，一行指令搞定）
ATCmd("AT+HTPSV=bin.bemfa.com,80\r\n", "OK", 500);
ATCmd("AT+S\r\n", "OK", 1000);
DownloadCmd("b/凭证=ota.bin", 6000);
```

ESP8266 版需要手动拼 HTTP 报文：
```c
// ESP8266 版（手动建 TCP + 手动发 HTTP GET）
// 1. 连 TCP
AT+CIPSTART="TCP","bin.bemfa.com",80
// 2. 指定发送长度
AT+CIPSEND=<HTTP报文长度>
// 3. 发送 HTTP GET 请求
GET /b/凭证=ota.bin HTTP/1.1\r\nHost: bin.bemfa.com\r\n\r\n
// 4. +IPD,n:数据  分段接收，解析 Content-Length 和 \r\n\r\n
```

---

## 八、文件索引

| 文件 | 作用 |
|------|------|
| [OTA.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/OTA.c) | OTA 核心：下载+校验+搬运 |
| [OTA.h](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/OTA.h) | OTA 接口声明 |
| [UART2.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/UART2.c) | 4G 通信串口+下载中断接收 |
| [Address.h](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/Address.h) | Flash 地址定义 |
| [MyFLASH.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/System/MyFLASH.c) | 内部 Flash 读写驱动 |
| [W25Q64.c](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/Hardware/W25Q64.c) | 外部 Flash 驱动 |
| [main.c (BootLoader)](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/BootLoader/User/main.c) | BootLoader 主程序+跳转 |
| [main.c (UserApp)](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/UserApplication_v1/User/main.c) | 用户程序+向量表设置 |
| [HTTP 样本](file:///d:/develop/linux/public/YQ_VScode/JieDuan_4/new_stm32/stm32风扇/OTA_bemfa.com/UserApplication_v1_HTTP.txt) | HTTP 响应报文实例 |
