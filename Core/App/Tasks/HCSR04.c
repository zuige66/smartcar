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
uint8_t echo_flag = 0;
uint32_t rising_cnt = 0;    // Echo上升沿捕获计数值
uint32_t falling_cnt = 0;   // Echo下降沿捕获计数值

void StartHCSR04Task(void *argument) {

    float distance = 0.0;
    for(;;)
    {
        // 1. 发送Trig触发脉冲（和你原来代码时序完全一致）
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port,HCSR04_Trig_Pin, GPIO_PIN_SET);
        osDelay(5);
        HAL_GPIO_WritePin(HCSR04_Trig_GPIO_Port,HCSR04_Trig_Pin, GPIO_PIN_RESET);

        // 2. 每次测量前清空所有计数、标志位、定时器计数值
        rising_cnt = 0;
        falling_cnt = 0;
        echo_flag = 0;
        __HAL_TIM_SET_COUNTER(&htim1, 0);   // 定时器计数器清零

        // 3. 配置上升沿捕获，开启输入捕获中断
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
        HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3);

        // 4. FreeRTOS非阻塞等待捕获完成（超时150ms防止卡死任务）
        uint8_t wait_timeout = 0;
        while( echo_flag != 2 && wait_timeout < 150 )
        {
            osDelay(1);   // 用FreeRTOS任务延时，不会卡死整个系统！！！
            wait_timeout++;
        }

        // 5. 强制停止捕获，防止中断重复触发
        HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_3);

        // 6. 距离计算（100%保留你原本的公式！配置完TIM1后完全精准）
        // 原理：(结束计数值-开始计数值) = 高电平时间 单位us
        // 距离cm = 时间us * 0.34(声速mm/us) / 2(往返) /10 =  *0.017
        if(wait_timeout < 150) // 捕获有效，正常计算
        {

            distance = (float)(falling_cnt - rising_cnt) * 0.017f;
            // 拆分整数、小数部分，全程只用整数打印，彻底杜绝%f浮点数爆栈问题
            uint32_t dist_int  = (uint32_t)distance;        // 距离整数部分  6
            uint32_t dist_dec  = (uint32_t)((distance - dist_int)*100); // 距离小数部分 83

            // 极小安全数组，FreeRTOS任务栈绝对安全，绝不卡死系统
            char debug_msg[80];
            // 格式完美：整数.两位小数，无多余0，格式完整，全程只用%d整数格式符，完全不碰%f
            int len = sprintf(debug_msg, "shortDebug: rising=%lu, falling=%lu, distance=%u.%02u cm\r\n",
                                rising_cnt, falling_cnt, dist_int, dist_dec);
            //HAL_UART_Transmit(&huart2, (uint8_t *)debug_msg, len, HAL_MAX_DELAY);
        }
        // else // 超时无回波，超量程标记
        // {
        //     distance = 999.9f;
        //     // 发送超时信息
        //     char timeout_msg[100] ;
        //     int len = sprintf(timeout_msg,"longDebug: timeout, no echo received\r\n rising=%lu, falling=%lu", rising_cnt, falling_cnt);
        //     HAL_UART_Transmit(&huart2, (uint8_t *)timeout_msg, len, HAL_MAX_DELAY);
        // }


        // 7. 发送距离数据到FreeRTOS距离队列
        osMessageQueuePut(DistanceHandle, &distance, 0, 100);

        // 8. 测量间隔延时，防止超声波余波干扰
        osDelay(100);
    }
    /* USER CODE END StartHCSP04Task */
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM1)  // 判断是TIM1定时器的中断
    {
        if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) // 判断是通道3
        {
            if(echo_flag == 0)
            {
                // 第一次中断：Echo上升沿，读取计数值
                rising_cnt = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_3);
                // 切换捕获极性为下降沿，等待高电平结束
                __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_FALLING);
                echo_flag = 1;
            }
            else if(echo_flag == 1)
            {
                // 第二次中断：Echo下降沿，读取计数值
                falling_cnt = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_3);
                // 恢复默认上升沿配置
                __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
                echo_flag = 2; // 标记捕获全部完成
            }
        }
    }
}