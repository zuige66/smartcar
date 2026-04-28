/**
  ******************************************************************************
  * @file    Encoder.c
  * @brief   编码器驱动
  * @details 使用TIM3编码器模式读取电机转速
  *          引脚：PB4(TIM3_CH1), PB5(TIM3_CH2) - 部分重映射
  ******************************************************************************
  */

#include "Encoder.h"
#include "tim.h"

// 外部定时器句柄声明
extern TIM_HandleTypeDef htim3;

/**
  * @brief  初始化编码器
  * @details 启动TIM3编码器模式
  */
void Encoder_Init(void) {
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
}

/**
  * @brief  获取编码器速度
  * @retval 编码器脉冲数（有符号）
  * @note   正值表示正转，负值表示反转
  */
int32_t Encoder_GetSpeed(void) {
    int16_t count = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
    return (int32_t)count;
}

/**
  * @brief  重置编码器计数器
  */
void Encoder_Reset(void) {
    __HAL_TIM_SET_COUNTER(&htim3, 0);
}
