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
extern osSemaphoreId_t EchoBinarySemHandle;

uint8_t echo_flag = 0;
uint32_t rising_cnt = 0;
uint32_t falling_cnt = 0;

void StartHCSR04Task(void *argument) {

    float distance = 0.0f;
    for(;;)
    {
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_SET);
        osDelay(5);
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port, HCSR04_Trig_Pin, GPIO_PIN_RESET);

        rising_cnt = 0;
        falling_cnt = 0;
        echo_flag = 0;
        __HAL_TIM_SET_COUNTER(&htim1, 0);

        __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
        HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3);

        osSemaphoreAcquire(EchoBinarySemHandle, 150);
        HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_3);

        if(echo_flag == 2)
        {
            distance = (float)(falling_cnt - rising_cnt) * 0.017f;
        }
        else
        {
            distance = 999.9f;
        }

        osMessageQueuePut(DistanceHandle, &distance, 0, 100);
        osDelay(100);
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM1)
    {
        if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
        {
            if(echo_flag == 0)
            {
                rising_cnt = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_3);
                __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_FALLING);
                echo_flag = 1;
            }
            else if(echo_flag == 1)
            {
                falling_cnt = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_3);
                __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
                echo_flag = 2;
                osSemaphoreRelease(EchoBinarySemHandle);
            }
        }
    }
}