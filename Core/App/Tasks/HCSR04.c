/**
 * HC-SR04 超声波测距 — 与参考代码完全一致的实现
 *
 * 保留全局变量接口供其他任务读取。
 */

#include "cmsis_os2.h"
#include "main.h"
#include "stm32f1xx_hal_gpio.h"
#include "tim.h"
#include <stm32f1xx_hal_tim.h>

// ===== 共享变量 =====
volatile float    g_distance    = 0.0f;
volatile uint8_t  g_ic_state    = 0;
volatile uint32_t g_ic_duration = 0;
volatile uint8_t  g_ic_timeout  = 0;

// ===== RTOS =====
extern osSemaphoreId_t EchoBinarySemHandle;

// ===== TIM =====
extern TIM_HandleTypeDef htim1;

// ===== 中断内部变量 =====
static volatile uint8_t  echo_flag    = 0;
static volatile uint32_t rising_cnt   = 0;
static volatile uint32_t falling_cnt  = 0;

// ===================================================================
// TIM1 输入捕获中断回调
// ===================================================================
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM1) return;
    if (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_3) return;

    if (echo_flag == 0) {
        rising_cnt = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_3);
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_FALLING);
        echo_flag = 1;
    } else if (echo_flag == 1) {
        falling_cnt = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_3);
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
        echo_flag = 2;
        osSemaphoreRelease(EchoBinarySemHandle);
    }
}

// ===================================================================
// 超声波任务
// ===================================================================
void StartHCSR04Task(void *argument) {
    (void)argument;

    for (;;) {
        // ── 0. 排空残留信号量（防止上次周期迟到ISR造成误触发） ──
        osSemaphoreAcquire(EchoBinarySemHandle, 0);

        // ── 1. Trig 脉冲 10-20µs ──
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_SET);
        for (volatile uint32_t i = 0; i < 500; i++);   // ~15µs @72MHz
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);

        // ── 2. 重置状态，立即启动捕获（必须在 Echo 返回之前准备好） ──
        rising_cnt  = 0;
        falling_cnt = 0;
        echo_flag   = 0;
        g_ic_state  = 1;

        __HAL_TIM_SET_COUNTER(&htim1, 0);
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
        HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3);

        // ── 3. 等信号量（ISR 释放）或超时 150ms ──
        osSemaphoreAcquire(EchoBinarySemHandle, 150);
        HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_3);

        // ── 4. 结果 ──
        if (echo_flag == 2) {
            uint32_t duration = falling_cnt - rising_cnt;
            g_ic_duration = duration;
            g_ic_state = 3;

            if (duration >= 117 && duration <= 23529) {
                g_distance = (float)duration * 0.017f;
            } else {
                g_distance = 0.0f;
            }
            g_ic_timeout = 0;
        } else {
            g_ic_timeout++;
            if (g_ic_timeout > 5) g_ic_timeout = 5;
            g_distance = 0.0f;
            g_ic_state = 0;
            g_ic_duration = 0;
        }

        // ── 5. 间隔 ──
        osDelay(100);
    }
}
