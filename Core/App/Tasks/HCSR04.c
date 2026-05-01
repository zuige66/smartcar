#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "main.h"
#include "stm32f1xx_hal_gpio.h"
#include "tim.h"
#include "usart.h"
#include <stm32f1xx_hal_tim.h>

#include "oled.h"

extern osMessageQueueId_t DistanceHandle;

void StartHCSR04Task(void *argument) {

    float distance = 0.0f;

    // 启动TIM1计数器（只需启动一次）
    HAL_TIM_Base_Start(&htim1);

    for(;;)
    {
        // 1. 发送Trig脉冲（至少10us）
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);
        osDelay(1);
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_SET);
        osDelay(1);
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);

        // 2. 轮询方式等待Echo上升沿
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        uint32_t timeout_start = HAL_GetTick();
        uint32_t rising = 0, falling = 0;

        // 等待上升沿（Echo变高）
        while (HAL_GPIO_ReadPin(HCSR04_Echo_GPIO_Port, HCSR04_Echo_Pin) == GPIO_PIN_RESET) {
            if (HAL_GetTick() - timeout_start > 50) {
                goto sensor_fail;  // 超时
            }
        }
        rising = __HAL_TIM_GET_COUNTER(&htim1);

        // 等待下降沿（Echo变低）
        timeout_start = HAL_GetTick();
        while (HAL_GPIO_ReadPin(HCSR04_Echo_GPIO_Port, HCSR04_Echo_Pin) == GPIO_PIN_SET) {
            if (HAL_GetTick() - timeout_start > 50) {
                goto sensor_fail;  // 超时
            }
        }
        falling = __HAL_TIM_GET_COUNTER(&htim1);

        // 3. 计算距离（1MHz时钟，每tick=1us，声速340m/s）
        //    距离 = 时间(us) * 0.034 / 2 = 时间(us) * 0.017
        distance = (float)(falling - rising) * 0.017f;

        // 合理性检查：2cm ~ 400cm
        if (distance < 2.0f || distance > 400.0f) {
            distance = 0.0f;
        }

        osMessageQueuePut(DistanceHandle, &distance, 0, 100);
        osDelay(100);
        continue;

sensor_fail:
        distance = 0.0f;
        osMessageQueuePut(DistanceHandle, &distance, 0, 100);
        osDelay(100);
    }
}
