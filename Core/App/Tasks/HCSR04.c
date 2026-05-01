#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "main.h"
#include "stm32f1xx_hal_gpio.h"
#include "tim.h"
#include "usart.h"
#include <stm32f1xx_hal_tim.h>

#include "oled.h"

// 共享距离变量（只写，由CtrlTask/OledTask/UartTask读取）
volatile float g_distance = 0.0f;

void StartHCSR04Task(void *argument) {

    float distance = 0.0f;

    // 启动TIM1计数器（只需启动一次）
    HAL_TIM_Base_Start(&htim1);

    for(;;)
    {
        // 1. 先确保Echo引脚为低电平（上次测量残留检查）
        //    如果Echo卡在高电平超过50ms，说明传感器异常
        {
            uint32_t wait_low_start = HAL_GetTick();
            while (HAL_GPIO_ReadPin(HCSR04_Echo_GPIO_Port, HCSR04_Echo_Pin) == GPIO_PIN_SET) {
                if (HAL_GetTick() - wait_low_start > 50) {
                    goto sensor_fail;
                }
            }
        }

        // 2. 发送Trig脉冲（至少10us）
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);
        osDelay(1);
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_SET);
        osDelay(1);
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);

        // 3. 轮询方式等待Echo上升沿
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        uint32_t timeout_start = HAL_GetTick();
        uint32_t rising = 0, falling = 0;

        // 等待上升沿（Echo变高）
        while (HAL_GPIO_ReadPin(HCSR04_Echo_GPIO_Port, HCSR04_Echo_Pin) == GPIO_PIN_RESET) {
            if (HAL_GetTick() - timeout_start > 30) {
                goto sensor_fail;  // 超时
            }
        }
        rising = __HAL_TIM_GET_COUNTER(&htim1);

        // 等待下降沿（Echo变低）
        timeout_start = HAL_GetTick();
        while (HAL_GPIO_ReadPin(HCSR04_Echo_GPIO_Port, HCSR04_Echo_Pin) == GPIO_PIN_SET) {
            if (HAL_GetTick() - timeout_start > 30) {
                goto sensor_fail;  // 超时
            }
        }
        falling = __HAL_TIM_GET_COUNTER(&htim1);

        // 4. 计算距离（1MHz时钟，每tick=1us，声速340m/s）
        //    距离 = 时间(us) * 0.034 / 2 = 时间(us) * 0.017
        {
            uint32_t duration = falling - rising;
            // 合理性：2cm~400cm → 117us~23529us
            if (duration < 117 || duration > 23529) {
                distance = 0.0f;
            } else {
                distance = (float)duration * 0.017f;
            }
        }

        g_distance = distance;
        osDelay(80);  // 测量间隔约80ms（总计约100ms周期）
        continue;

sensor_fail:
        distance = 0.0f;
        g_distance = 0.0f;
        osDelay(80);
    }
}
