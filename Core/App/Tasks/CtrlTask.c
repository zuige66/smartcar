/**
  ******************************************************************************
  * @file    CtrlTask.c
  * @brief   综合控制任务
  * @details 决策中枢：读取超声波和循迹传感器数据，使用PID控制电机
  ******************************************************************************
  */

#include "cmsis_os2.h"
#include "pid.h"
#include "Encoder.h"

// 外部变量声明
extern osMessageQueueId_t DistanceHandle;
extern osMessageQueueId_t TrackHandle;
extern osMessageQueueId_t MotorActionHandle;

// 电机控制指令枚举
typedef enum {
    MOTOR_CMD_STOP = 0,      // 停止
    MOTOR_CMD_FORWARD,       // 前进
    MOTOR_CMD_TURN_LEFT,     // 左转
    MOTOR_CMD_TURN_RIGHT     // 右转
} MotorCmdTypeDef;

// 电机控制指令结构体
typedef struct {
    MotorCmdTypeDef cmd;     // 动作指令
    uint16_t pwm;            // 速度PWM值 0-1000
} MotorActionMsg;

// PID控制器实例
static PID_HandleTypeDef speed_pid;   // 速度PID
static PID_HandleTypeDef turn_pid;    // 转向PID

// 目标速度
#define TARGET_SPEED 500.0f  // 目标速度（编码器脉冲数）

/**
  * @brief  计算循迹偏差
  * @param  track_data: 四路循迹传感器状态 (0x00-0x0F)
  * @retval 偏差值：负数=偏左，正数=偏右，0=居中
  */
static int8_t CalculateTrackError(uint8_t track_data) {
    switch(track_data) {
        // 居中状态
        case 0x06:  // 0110 - L1+R1
        case 0x09:  // 1001 - L0+R2
        case 0x05:  // 0101 - L1+R1
        case 0x0A:  // 1010 - R1+R2
            return 0;

        // 稍微偏左
        case 0x02:  // 0010 - L1
            return -1;

        // 偏左
        case 0x01:  // 0001 - L0
        case 0x03:  // 0011 - L0+L1
            return -2;

        // 严重偏左
        case 0x0D:  // 1101 - L0+L1+R2
        case 0x0E:  // 1110 - L0+L1+R1+R2
            return -3;

        // 稍微偏右
        case 0x04:  // 0100 - R1
            return 1;

        // 偏右
        case 0x08:  // 1000 - R2
        case 0x0C:  // 1100 - R1+R2
            return 2;

        // 严重偏右
        case 0x0B:  // 1011 - L0+R1+R2
            return 3;

        // 全检测到或全未检测到
        case 0x00:  // 0000 - 全未检测到
        case 0x0F:  // 1111 - 全检测到
        default:
            return 0;
    }
}

/**
  * @brief  综合控制任务
  * @param  argument: 任务参数（未使用）
  * @details 读取传感器数据，使用PID控制电机
  */
void StartCtrlTask(void *argument) {
    (void) argument;

    float distance = 0.0f;
    uint8_t track_data = 0;
    MotorActionMsg motor_cmd;
    int32_t encoder_speed = 0;
    int8_t track_error = 0;

    // 初始化PID控制器
    PID_Init(&speed_pid, 2.0f, 0.1f, 0.5f);  // 速度PID: Kp, Ki, Kd
    PID_SetTarget(&speed_pid, TARGET_SPEED);
    PID_SetOutputLimits(&speed_pid, 0, 1000);

    PID_Init(&turn_pid, 1.5f, 0.05f, 0.3f);  // 转向PID: Kp, Ki, Kd
    PID_SetTarget(&turn_pid, 0);  // 目标：偏差为0（居中）
    PID_SetOutputLimits(&turn_pid, -500, 500);

    // 初始化编码器
    Encoder_Init();

    // 初始状态：停止
    motor_cmd.cmd = MOTOR_CMD_STOP;
    motor_cmd.pwm = 0;

    for (;;) {
        // 1. 读取传感器数据（非阻塞）
        osMessageQueueGet(DistanceHandle, &distance, 0, 10);
        osMessageQueueGet(TrackHandle, &track_data, 0, 10);

        // 2. 读取编码器速度并重置
        encoder_speed = Encoder_GetSpeed();
        Encoder_Reset();

        // 3. 计算循迹偏差
        track_error = CalculateTrackError(track_data);

        // 4. 避障逻辑（最高优先级）
        if (distance > 0 && distance <= 10.0f) {
            motor_cmd.cmd = MOTOR_CMD_STOP;
            motor_cmd.pwm = 0;
            PID_Reset(&speed_pid);  // 停止时重置PID
            PID_Reset(&turn_pid);
        }
        // 5. PID控制逻辑
        else {
            // 计算速度PID输出
            float speed_output = PID_Compute(&speed_pid, (float)encoder_speed);

            // 计算转向PID输出
            float turn_output = PID_Compute(&turn_pid, (float)track_error);

            // 根据转向偏差决定动作
            if (track_error < -1) {
                motor_cmd.cmd = MOTOR_CMD_TURN_LEFT;
                motor_cmd.pwm = (uint16_t)speed_output;
            } else if (track_error > 1) {
                motor_cmd.cmd = MOTOR_CMD_TURN_RIGHT;
                motor_cmd.pwm = (uint16_t)speed_output;
            } else {
                motor_cmd.cmd = MOTOR_CMD_FORWARD;
                motor_cmd.pwm = (uint16_t)speed_output;
            }

            // 确保PWM值有效
            if (motor_cmd.pwm < 100) {
                motor_cmd.pwm = 100;  // 最小速度
            }
        }

        // 6. 发送指令到电机驱动任务
        osMessageQueuePut(MotorActionHandle, &motor_cmd, 0, 5);

        // 7. 控制周期：30ms
        osDelay(30);
    }
}
