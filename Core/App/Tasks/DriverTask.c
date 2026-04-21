/**
  ******************************************************************************
  * @file    DriverTask.c
  * @brief   电机驱动任务文件
  * @details 实现双电机驱动的控制功能，包括PWM调速、方向控制等
  ******************************************************************************
  */

#include "stm32f1xx_hal.h"
#include "cmsis_os2.h"
#include "tim.h"
#include "motor.h"
#include "main.h"

// 外部变量声明
extern osMessageQueueId_t DriverPWMHandle;  // 电机PWM控制消息队列
extern osThreadId_t DriverTaskHandle;       // 电机驱动任务句柄

/**
  * @brief  配置电机控制GPIO引脚
  * @details 初始化PA4-PA7引脚为推挽输出模式，用于控制电机方向
  *          PA4: 左电机正转(AIN1), PA5: 左电机反转(AIN2)
  *          PA6: 右电机正转(BIN1), PA7: 右电机反转(BIN2)
  */
static void Motor_ConfigGPIO(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 使能GPIOA时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // 配置电机控制引脚为推挽输出模式
    GPIO_InitStruct.Pin = AIN1_Pin | AIN2_Pin | BIN1_Pin | BIN2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   // 推挽输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;           // 无上拉下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // 低速
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 初始化所有电机控制引脚为高电平（电机停止状态）
    HAL_GPIO_WritePin(GPIOA, AIN1_Pin | AIN2_Pin | BIN1_Pin | BIN2_Pin, GPIO_PIN_SET);
}

/**
  * @brief  启动电机PWM输出
  * @details 启动TIM2的通道1和通道2的PWM输出
  *          TIM2_CH1: 左电机PWM, TIM2_CH2: 右电机PWM
  */
static void Motor_StartPWM(void) {
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);  // 启动左电机PWM
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);  // 启动右电机PWM
}

/**
  * @brief  电机驱动初始化
  * @details 配置GPIO和PWM，初始化电机为停止状态
  */
void MotorDriver_Init(void) {
    Motor_ConfigGPIO();  // 配置电机控制引脚
    Motor_StartPWM();    // 启动PWM输出
    Motor_Stop(MOTOR_LEFT);   // 停止左电机
    Motor_Stop(MOTOR_RIGHT);  // 停止右电机
}

/**
  * @brief  设置电机速度
  * @param  motor: 电机ID (MOTOR_LEFT 或 MOTOR_RIGHT)
  * @param  speed: 速度值 (0-1000)
  * @details 通过设置PWM占空比来控制电机速度
  *          速度值越大，PWM占空比越高，电机转速越快
  */
void Motor_SetSpeed(MotorIdTypeDef motor, uint16_t speed) {
    // 限制最大速度为1000
    if (speed > 1000) speed = 1000;
    
    // 将速度值转换为PWM占空比值
    uint16_t pwm_val = (speed * PWM_MAX_VALUE) / 1000;

    switch (motor) {
        case MOTOR_LEFT:
            // 设置左电机PWM占空比 (TIM2_CH1)
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_val);
            break;
        case MOTOR_RIGHT:
            // 设置右电机PWM占空比 (TIM2_CH2)
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
  * @details 通过控制H桥电路的输入引脚来改变电机旋转方向
  *          正转: 正转引脚高电平，反转引脚低电平
  *          反转: 正转引脚低电平，反转引脚高电平
  */
void Motor_SetDirection(MotorIdTypeDef motor, MotorDirTypeDef dir) {
    GPIO_TypeDef* port = GPIOA;
    uint16_t pin1, pin2;  // pin1: 正转控制引脚, pin2: 反转控制引脚

    // 根据电机ID设置对应的控制引脚
    if (motor == MOTOR_LEFT)  { pin1 = AIN1_Pin; pin2 = AIN2_Pin; }  // 左电机 (AIN1, AIN2)
    else                     { pin1 = BIN1_Pin; pin2 = BIN2_Pin; }  // 右电机 (BIN1, BIN2)

    if (dir == MOTOR_FORWARD) {
        // 正转: 正转引脚高电平，反转引脚低电平
        HAL_GPIO_WritePin(port, pin1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(port, pin2, GPIO_PIN_RESET);
    } else {
        // 反转: 正转引脚低电平，反转引脚高电平
        HAL_GPIO_WritePin(port, pin1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(port, pin2, GPIO_PIN_SET);
    }
}

/**
  * @brief  停止电机
  * @param  motor: 电机ID (MOTOR_LEFT 或 MOTOR_RIGHT)
  * @details 通过设置PWM占空比为0和控制引脚为高电平来停止电机
  */
void Motor_Stop(MotorIdTypeDef motor) {
    if (motor == MOTOR_LEFT) {
        // 停止左电机: 设置PWM为0，两个方向引脚都为高电平
        HAL_GPIO_WritePin(GPIOA, AIN1_Pin | AIN2_Pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    } else {
        // 停止右电机: 设置PWM为0，两个方向引脚都为高电平
        HAL_GPIO_WritePin(GPIOA, BIN1_Pin | BIN2_Pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    }
}

/**
  * @brief  电机驱动任务
  * @param  argument: 任务参数（未使用）
  * @details FreeRTOS任务，循环执行电机控制动作
  *          演示动作: 前进2秒 -> 停止1秒 -> 后退2秒 -> 循环
  */
void StartDriverTask(void *argument) {
    (void) argument;  // 避免编译器警告

    // 初始化电机驱动
    MotorDriver_Init();
    
    // 设置电机速度 (0-1000)
    uint16_t speed = 800;
    uint32_t msg;  // 用于存储PWM值的消息变量

    // 无限循环执行电机控制动作
    for (;;) {
        // 前进: 两个电机都设置为前进方向
        Motor_SetDirection(MOTOR_LEFT, MOTOR_FORWARD);
        Motor_SetDirection(MOTOR_RIGHT, MOTOR_FORWARD);
        Motor_SetSpeed(MOTOR_LEFT, speed);
        Motor_SetSpeed(MOTOR_RIGHT, speed);
        msg = speed;  // 更新消息内容为当前速度值
        osMessageQueuePut(DriverPWMHandle, &msg, 0, 0);  // 发送PWM值到消息队列
        osDelay(3000);  // 延时2秒

       // 停止: 停止两个电机
       Motor_Stop(MOTOR_LEFT);
       Motor_Stop(MOTOR_RIGHT);
       msg = 0;  // 停止时速度为0
       //osMessageQueuePut(DriverPWMHandle, &msg, 0, 0);  // 发送PWM值到消息队列
       osDelay(1000);  // 延时1秒

        // 后退: 两个电机都设置为后退方向
        Motor_SetDirection(MOTOR_LEFT, MOTOR_BACKWARD);
        Motor_SetDirection(MOTOR_RIGHT, MOTOR_BACKWARD);
        Motor_SetSpeed(MOTOR_LEFT, speed);
        Motor_SetSpeed(MOTOR_RIGHT, speed);
        msg = speed;  // 更新消息内容为当前速度值
       //osMessageQueuePut(DriverPWMHandle, &msg, 0, 0);  // 发送PWM值到消息队列
        osDelay(2000);  // 延时2秒
    }
}
