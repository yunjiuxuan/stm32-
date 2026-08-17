#include "delay.h"

/* SystemCoreClock is defined in system_stm32f4xx.c (168000000 for STM32F407) */
extern uint32_t SystemCoreClock;

/* ===================================================================
 * DWT (Data Watchpoint and Trace) cycle counter
 * Used for FreeRTOS-safe microsecond delays.
 * Unlike SysTick, DWT does NOT interfere with the FreeRTOS tick.
 *
 * 为什么不用vTaskDelay替代delay_us:
 *   - vTaskDelay最小粒度=1ms=1000us，软件I2C需要5us精度
 *   - vTaskDelay只能在任务上下文调用，delay_us需要在ISR中也能安全使用
 *   - DWT是纯硬件计数器，不需要任何中断，完全独立于FreeRTOS
 * =================================================================== */
#define DEMCR           (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL        (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT      (*(volatile uint32_t *)0xE0001004)

void Delay_Init(void)
{
    /* Enable DWT cycle counter (FreeRTOS-safe, no SysTick conflict) */
    DEMCR |= (1 << 24);       /* TRCENA: enable trace/DWT block */
    DWT_CYCCNT = 0;           /* Reset cycle counter to 0 */
    DWT_CTRL |= (1 << 0);     /* CYCCNTENA: enable cycle counting */
}

/* Microsecond delay using DWT cycle counter (FreeRTOS-safe)
 * Does NOT touch SysTick registers, so FreeRTOS tick is unaffected.
 * Safe to call from both task and ISR context. */
void delay_us(int nus)
{
    uint32_t start = DWT_CYCCNT;
    uint32_t ticks = (uint32_t)nus * (SystemCoreClock / 1000000);
    while((DWT_CYCCNT - start) < ticks);
}

/* Millisecond delay (busy-wait, FreeRTOS-safe)
 * NOTE: In task context, prefer vTaskDelay(pdMS_TO_TICKS(nms))
 * which yields the CPU. This function busy-waits.
 * Only use this when vTaskDelay is not available (ISR / init before scheduler). */
void delay_ms(int nms)
{
    while(nms--)
        delay_us(1000);
}

/* Second delay (busy-wait, FreeRTOS-safe)
 * NOTE: In task context, prefer vTaskDelay(pdMS_TO_TICKS(1000)) */
void delay_s(int ns)
{
    while(ns--)
        delay_ms(1000);
}
