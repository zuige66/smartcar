#ifndef __MOTOR_H__
#define __MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

// 电机ID定义
typedef enum {
    MOTOR_LEFT = 0,
    MOTOR_RIGHT = 1
} MotorIdTypeDef;

// 电机方向定义
typedef enum {
    MOTOR_FORWARD = 0,
    MOTOR_BACKWARD = 1
} MotorDirTypeDef;

// PWM最大值
#define PWM_MAX_VALUE 1000

// 函数声明
void MotorDriver_Init(void);
void Motor_SetSpeed(MotorIdTypeDef motor, uint16_t speed);
void Motor_SetDirection(MotorIdTypeDef motor, MotorDirTypeDef dir);
void Motor_Stop(MotorIdTypeDef motor);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_H__ */