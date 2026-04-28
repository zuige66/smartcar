/**
  ******************************************************************************
  * @file    pid.c
  * @brief   PID控制算法实现
  * @details 增量式PID，支持输出限幅和积分抗饱和
  ******************************************************************************
  */

#include "pid.h"

/**
  * @brief  初始化PID控制器
  * @param  pid: PID控制器句柄
  * @param  Kp: 比例系数
  * @param  Ki: 积分系数
  * @param  Kd: 微分系数
  */
void PID_Init(PID_HandleTypeDef *pid, float Kp, float Ki, float Kd) {
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->setpoint = 0;
    pid->integral = 0;
    pid->prev_error = 0;
    pid->output_min = -1000;
    pid->output_max = 1000;
}

/**
  * @brief  设置目标值
  * @param  pid: PID控制器句柄
  * @param  target: 目标值
  */
void PID_SetTarget(PID_HandleTypeDef *pid, float target) {
    pid->setpoint = target;
}

/**
  * @brief  设置输出限幅
  * @param  pid: PID控制器句柄
  * @param  min: 输出下限
  * @param  max: 输出上限
  */
void PID_SetOutputLimits(PID_HandleTypeDef *pid, float min, float max) {
    pid->output_min = min;
    pid->output_max = max;
}

/**
  * @brief  计算PID输出
  * @param  pid: PID控制器句柄
  * @param  measurement: 当前测量值
  * @retval PID输出值
  */
float PID_Compute(PID_HandleTypeDef *pid, float measurement) {
    // 计算误差
    float error = pid->setpoint - measurement;

    // 积分项（带抗饱和）
    pid->integral += error;
    if (pid->integral > pid->output_max) {
        pid->integral = pid->output_max;
    } else if (pid->integral < pid->output_min) {
        pid->integral = pid->output_min;
    }

    // 微分项
    float derivative = error - pid->prev_error;
    pid->prev_error = error;

    // PID输出
    float output = (pid->Kp * error) + (pid->Ki * pid->integral) + (pid->Kd * derivative);

    // 输出限幅
    if (output > pid->output_max) {
        output = pid->output_max;
    } else if (output < pid->output_min) {
        output = pid->output_min;
    }

    return output;
}

/**
  * @brief  重置PID积分和误差
  * @param  pid: PID控制器句柄
  */
void PID_Reset(PID_HandleTypeDef *pid) {
    pid->integral = 0;
    pid->prev_error = 0;
}
