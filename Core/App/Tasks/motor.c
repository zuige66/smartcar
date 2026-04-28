/**
  ******************************************************************************
  * @file    motor.c
  * @brief   电机控制函数实现
  * @details 实现电机方向控制和速度控制
  ******************************************************************************
  */

#include "motor.h"
#include "stm32f1xx_hal.h"
#include "tim.h"
#include "main.h"

/**
  * @brief  设置电机速度
  * @param  motor: 电机ID (MOTOR_LEFT 或 MOTOR_RIGHT)
  * @param  speed: 速度值 (0-1000)
  */
void Motor_SetSpeed(MotorIdTypeDef motor, uint16_t speed) {
    if (speed > 1000) speed = 1000;

    uint16_t pwm_val = (speed * PWM_MAX_VALUE) / 1000;

    switch (motor) {
        case MOTOR_LEFT:
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_val);
            break;
        case MOTOR_RIGHT:
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pwm_val);
            break;
        default:
            break;
    }
}

/**
  * @brief  设置电机方向
  * @param  motor: 电机ID (MOTOR_LEFT 或 MOTOR_RIGHT)
  * @param  dir: 电机方向 (MOTOR_FORWARD 或 MOTOR_BACKWARD)
  */
void Motor_SetDirection(MotorIdTypeDef motor, MotorDirTypeDef dir) {
    GPIO_TypeDef* port = GPIOA;
    uint16_t pin1, pin2;

    if (motor == MOTOR_LEFT) {
        pin1 = AIN1_Pin;
        pin2 = AIN2_Pin;
    } else {
        pin1 = BIN1_Pin;
        pin2 = BIN2_Pin;
    }

    if (dir == MOTOR_FORWARD) {
        HAL_GPIO_WritePin(port, pin1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(port, pin2, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(port, pin1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(port, pin2, GPIO_PIN_SET);
    }
}

/**
  * @brief  停止电机
  * @param  motor: 电机ID (MOTOR_LEFT 或 MOTOR_RIGHT)
  */
void Motor_Stop(MotorIdTypeDef motor) {
    if (motor == MOTOR_LEFT) {
        HAL_GPIO_WritePin(GPIOA, AIN1_Pin | AIN2_Pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    } else {
        HAL_GPIO_WritePin(GPIOA, BIN1_Pin | BIN2_Pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    }
}
