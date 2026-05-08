/**
 * HC-SR04 超声波测距 — TIM1 输入捕获中断 + 5次采样去极值
 *
 * 中断方案：
 *   - TIM1 CH3 输入捕获，上升沿记录时刻，切换到下降沿
 *   - 下降沿计算脉宽，信号量通知任务
 *   - 最小脉宽检查（<100µs = 噪声，丢弃）
 *   - 不使用 HAL_TIM_IC_Stop_IT / Start_IT，手动控制中断使能
 */

#include "cmsis_os2.h"
#include "main.h"
#include "stm32f1xx_hal_gpio.h"
#include "tim.h"
#include <stm32f1xx_hal_tim.h>
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>

#define HCSR04_SAMPLES 5
#define MIN_PULSE_US   100  // 最小有效脉宽，过滤噪声

// ===== 共享变量 =====
volatile float    g_distance    = 0.0f;
volatile uint8_t  g_ic_state    = 0;
volatile uint32_t g_ic_duration = 0;
volatile uint8_t  g_ic_timeout  = 0;
volatile uint32_t g_ic_timeout_total = 0;
volatile uint32_t g_ic_success_total = 0;

extern TIM_HandleTypeDef htim1;
extern UART_HandleTypeDef huart2;
extern osSemaphoreId_t EchoBinarySemHandle;

// ===== ISR 内部变量 =====
static volatile uint32_t ic_rising_val = 0;
static volatile uint32_t ic_duration_isr = 0;
static volatile uint8_t  ic_capture_state = 0;  // 0=等待上升沿 1=已捕获上升沿 2=完成

// 微秒延时（72MHz主频，更准确的实现）
static void delay_us(uint32_t us) {
    uint32_t ticks = us * 72;  // 72MHz = 72 ticks/us
    uint32_t told = SysTick->VAL;
    uint32_t tnow, tcnt = 0;
    while (tcnt < ticks) {
        tnow = SysTick->VAL;
        if (tnow != told) {
            if (tnow < told) {
                tcnt += told - tnow;
            } else {
                tcnt += SysTick->LOAD - tnow + told;
            }
            told = tnow;
        }
    }
}

// 调试输出函数
static void dbg_printf(const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)n, 100);
    }
}

// ===================================================================
// TIM1 输入捕获中断回调（上升沿 → 切换下降沿 → 完成）
// ===================================================================
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM1) return;

    uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);

    if (ic_capture_state == 0) {
        // 上升沿：记录时刻，切换到下降沿
        ic_rising_val = capture;
        ic_capture_state = 1;
        dbg_printf("[ISR] RISING edge captured! val=%lu\r\n", capture);
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3,
                                      TIM_INPUTCHANNELPOLARITY_FALLING);
    } else if (ic_capture_state == 1) {
        // 下降沿：计算脉宽
        uint32_t falling = capture;
        uint32_t duration;
        if (falling >= ic_rising_val) {
            duration = falling - ic_rising_val;
        } else {
            duration = (0xFFFF - ic_rising_val) + falling + 1;
        }

        dbg_printf("[ISR] FALLING edge captured! val=%lu dur=%lu\r\n", falling, duration);

        // 最小脉宽检查：太短说明是噪声
        if (duration < MIN_PULSE_US) {
            dbg_printf("[ISR] Pulse too short (%luus), discarding\r\n", duration);
            ic_capture_state = 0;
            __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3,
                                          TIM_INPUTCHANNELPOLARITY_RISING);
            return;
        }

        ic_duration_isr = duration;
        ic_capture_state = 2;
        osSemaphoreRelease(EchoBinarySemHandle);
        // 切换回上升沿，准备下次捕获
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3,
                                      TIM_INPUTCHANNELPOLARITY_RISING);
        dbg_printf("[ISR] Capture complete! duration=%luus, polarity reset to RISING\r\n", duration);
    }
}

// ===================================================================
// 超声波任务：中断采集 + 5次采样去极值取平均
// ===================================================================
void StartHCSR04Task(void *argument) {
    (void)argument;

    // 启动 TIM1 输入捕获中断模式
    HAL_StatusTypeDef ret = HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3);
    
    // 强制设置上升沿捕获（修复HAL配置问题）
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
    
    // 关键状态检查（只输出一次）
    char buf[128];
    int n = sprintf(buf, "[HCSR04] IC_START=%d | CR1=%04X | CCER=%04X | SR=%04X\r\n",
                    ret, TIM1->CR1, TIM1->CCER, TIM1->SR);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, n, 100);

    for (;;) {
        float samples[HCSR04_SAMPLES];
        uint8_t valid = 0;
        uint8_t cycle_fail = 0;

        dbg_printf("[HCSR04] === New measurement cycle ===\r\n");

        for (int i = 0; i < HCSR04_SAMPLES; i++) {
            dbg_printf("[HCSR04] Sample %d/%d:\r\n", i+1, HCSR04_SAMPLES);

            // 检查TIM状态
            uint32_t cr1 = TIM1->CR1;
            uint32_t ccer = TIM1->CCER;
            uint32_t dier = TIM1->DIER;
            dbg_printf("[HCSR04]   TIM CR1=%04X CCER=%04X DIER=%04X\r\n", cr1, ccer, dier);

            // 1. 清空残留信号量
            int clear_cnt = 0;
            while (osSemaphoreAcquire(EchoBinarySemHandle, 0) == osOK) {
                clear_cnt++;
            }
            if (clear_cnt > 0) {
                dbg_printf("[HCSR04]   Cleared %d stale semaphores\r\n", clear_cnt);
            }

            // 2. 重置捕获状态
            ic_capture_state = 0;
            ic_duration_isr = 0;
            __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3,
                                          TIM_INPUTCHANNELPOLARITY_RISING);
            dbg_printf("[HCSR04]   State reset, polarity=RISING\r\n");

            // 3. 发送 Trig 脉冲 15µs
            dbg_printf("[HCSR04]   Sending Trig pulse...\r\n");
            HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);
            delay_us(5);
            HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_SET);
            delay_us(15);
            HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);
            dbg_printf("[HCSR04]   Trig pulse sent, waiting for Echo...\r\n");

            // 4. 等待信号量（测量完成），超时 100ms
            osStatus_t sem_st = osSemaphoreAcquire(EchoBinarySemHandle, 100);
            dbg_printf("[HCSR04]   Sem status: %d, State: %d\r\n", sem_st, ic_capture_state);

            if (sem_st == osOK && ic_capture_state == 2) {
                uint32_t duration = ic_duration_isr;
                g_ic_duration = duration;
                g_ic_state = 3;
                float dist_cm = (float)duration * 0.017f;
                if (duration >= 117 && duration <= 23529) {
                    samples[valid++] = dist_cm;
                    dbg_printf("[HCSR04]   SUCCESS! Duration=%luus, Distance=%.2fcm\r\n", duration, dist_cm);
                } else {
                    dbg_printf("[HCSR04]   Invalid range! Duration=%luus (out of 2-400cm)\r\n", duration);
                }
            } else {
                cycle_fail++;
                g_ic_timeout_total++;
                g_ic_state = 0;
                g_ic_duration = 0;
                dbg_printf("[HCSR04]   TIMEOUT! sem_st=%d, state=%d\r\n", sem_st, ic_capture_state);
                // 复位传感器
                HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_SET);
                delay_us(100);
                HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);
            }

            osDelay(80);
        }

        // ── 去极值取平均 ──
        if (valid >= 3) {
            for (int i = 0; i < valid - 1; i++)
                for (int j = 0; j < valid - i - 1; j++)
                    if (samples[j] > samples[j+1]) {
                        float t = samples[j]; samples[j] = samples[j+1]; samples[j+1] = t;
                    }
            float sum = 0;
            for (int i = 1; i < valid - 1; i++) sum += samples[i];
            g_distance = sum / (valid - 2);
            g_ic_timeout = 0;
            g_ic_success_total++;
        } else if (valid > 0) {
            float sum = 0;
            for (int i = 0; i < valid; i++) sum += samples[i];
            g_distance = sum / valid;
            g_ic_timeout = 0;
            g_ic_success_total++;
        } else {
            g_distance = 0.0f;
            g_ic_timeout = cycle_fail;
        }

        // UART 输出
        {
            char dbg[128];
            int dist_int = (int)g_distance;
            int dist_dec = (int)((g_distance - dist_int) * 100);
            int n = sprintf(dbg, "[HCSR04] dis:%d.%02dcm ok:%lu fail:%lu v:%d\r\n",
                           dist_int, dist_dec, g_ic_success_total, g_ic_timeout_total, valid);
            if (n > 0) {
                if (HAL_UART_Transmit(&huart2, (uint8_t *)dbg, (uint16_t)n, 100) != HAL_OK) {
                    HAL_UART_DeInit(&huart2);
                    MX_USART2_UART_Init();
                }
            }
        }
    }
}
