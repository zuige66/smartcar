#ifndef __PID_H__
#define __PID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// PID控制器结构体
typedef struct {
    float Kp;           // 比例系数
    float Ki;           // 积分系数
    float Kd;           // 微分系数
    float setpoint;     // 目标值
    float integral;     // 积分累计
    float prev_error;   // 上一次误差
    float output_min;   // 输出下限
    float output_max;   // 输出上限
} PID_HandleTypeDef;

// 初始化PID控制器
void PID_Init(PID_HandleTypeDef *pid, float Kp, float Ki, float Kd);

// 设置目标值
void PID_SetTarget(PID_HandleTypeDef *pid, float target);

// 设置输出限幅
void PID_SetOutputLimits(PID_HandleTypeDef *pid, float min, float max);

// 计算PID输出
float PID_Compute(PID_HandleTypeDef *pid, float measurement);

// 重置PID积分和误差
void PID_Reset(PID_HandleTypeDef *pid);

#ifdef __cplusplus
}
#endif

#endif /* __PID_H__ */
