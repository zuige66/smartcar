/**
  ******************************************************************************
  * @file    DriverTask.c
  * @brief   电机驱动任务
  * @details 从MotorAction队列读取控制指令，驱动电机执行相应动作
  ******************************************************************************
  */

#include "stm32f1xx_hal.h"
#include "cmsis_os2.h"
#include "tim.h"
#include "motor.h"
#include "main.h"

// 外部变量声明
extern osMessageQueueId_t MotorActionHandle;  // 电机控制指令队列

// 电机控制指令枚举（与CtrlTask保持一致）
typedef enum {
    MOTOR_CMD_STOP = 0,      // 停止
    MOTOR_CMD_FORWARD,       // 前进
    MOTOR_CMD_TURN_LEFT,     // 差速左转（循迹用）
    MOTOR_CMD_TURN_RIGHT,    // 差速右转（循迹用）
    MOTOR_CMD_SPIN_LEFT,     // 原地左转（左轮后退+右轮前进）
    MOTOR_CMD_SPIN_RIGHT     // 原地右转（左轮前进+右轮后退）
} MotorCmdTypeDef;

// 电机控制指令结构体（与CtrlTask保持一致）
typedef struct {
    MotorCmdTypeDef cmd;     // 动作指令
    uint16_t pwm;            // 速度PWM值 0-1000（避障用）
    uint16_t pwm_left;       // 左轮PWM（循迹差速用）
    uint16_t pwm_right;      // 右轮PWM（循迹差速用）
} MotorActionMsg;

/**
  * @brief  电机驱动初始化
  * @details 配置GPIO和PWM，初始化电机为停止状态
  */
void MotorDriver_Init(void) {
    // 使能GPIOA时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // 配置电机控制引脚
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = AIN1_Pin | AIN2_Pin | BIN1_Pin | BIN2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 初始化所有电机控制引脚为高电平（停止状态）
    HAL_GPIO_WritePin(GPIOA, AIN1_Pin | AIN2_Pin | BIN1_Pin | BIN2_Pin, GPIO_PIN_SET);

    // 启动PWM输出
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    // 停止电机
    Motor_Stop(MOTOR_LEFT);
    Motor_Stop(MOTOR_RIGHT);
}

/**
  * @brief  电机驱动任务
  * @param  argument: 任务参数（未使用）
  * @details 从MotorAction队列读取指令，控制电机执行相应动作
  */
void StartDriverTask(void *argument) {
    (void) argument;

    // 初始化电机驱动
    MotorDriver_Init();

    MotorActionMsg motor_cmd;

    for (;;) {
        // 从队列读取指令，超时100ms
        if (osMessageQueueGet(MotorActionHandle, &motor_cmd, NULL, 100) == osOK) {
            switch (motor_cmd.cmd) {
                case MOTOR_CMD_STOP:
                    Motor_Stop(MOTOR_LEFT);
                    Motor_Stop(MOTOR_RIGHT);
                    break;

                case MOTOR_CMD_FORWARD:
                    Motor_SetDirection(MOTOR_LEFT, MOTOR_FORWARD);
                    Motor_SetDirection(MOTOR_RIGHT, MOTOR_FORWARD);
                    Motor_SetSpeed(MOTOR_LEFT, motor_cmd.pwm_left);
                    Motor_SetSpeed(MOTOR_RIGHT, motor_cmd.pwm_right);
                    break;

                case MOTOR_CMD_TURN_LEFT:
                    // 左转：左轮慢，右轮快（差速转向）
                    Motor_SetDirection(MOTOR_LEFT, MOTOR_FORWARD);
                    Motor_SetDirection(MOTOR_RIGHT, MOTOR_FORWARD);
                    Motor_SetSpeed(MOTOR_LEFT, motor_cmd.pwm / 3);   // 内圈减速
                    Motor_SetSpeed(MOTOR_RIGHT, motor_cmd.pwm);
                    break;

                case MOTOR_CMD_TURN_RIGHT:
                    // 右转：左轮快，右轮慢（差速转向）
                    Motor_SetDirection(MOTOR_LEFT, MOTOR_FORWARD);
                    Motor_SetDirection(MOTOR_RIGHT, MOTOR_FORWARD);
                    Motor_SetSpeed(MOTOR_LEFT, motor_cmd.pwm);
                    Motor_SetSpeed(MOTOR_RIGHT, motor_cmd.pwm / 3);  // 内圈减速
                    break;

                case MOTOR_CMD_SPIN_LEFT:
                    // 原地左转：左轮后退，右轮前进
                    Motor_SetDirection(MOTOR_LEFT, MOTOR_BACKWARD);
                    Motor_SetDirection(MOTOR_RIGHT, MOTOR_FORWARD);
                    Motor_SetSpeed(MOTOR_LEFT, motor_cmd.pwm);
                    Motor_SetSpeed(MOTOR_RIGHT, motor_cmd.pwm);
                    break;

                case MOTOR_CMD_SPIN_RIGHT:
                    // 原地右转：左轮前进，右轮后退
                    Motor_SetDirection(MOTOR_LEFT, MOTOR_FORWARD);
                    Motor_SetDirection(MOTOR_RIGHT, MOTOR_BACKWARD);
                    Motor_SetSpeed(MOTOR_LEFT, motor_cmd.pwm);
                    Motor_SetSpeed(MOTOR_RIGHT, motor_cmd.pwm);
                    break;

                default:
                    Motor_Stop(MOTOR_LEFT);
                    Motor_Stop(MOTOR_RIGHT);
                    break;
            }
        }
    }
}
