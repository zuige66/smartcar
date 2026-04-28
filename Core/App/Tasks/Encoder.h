#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

// 编码器初始化
void Encoder_Init(void);

// 获取编码器速度（脉冲数，正值表示正转，负值表示反转）
int32_t Encoder_GetSpeed(void);

// 重置编码器计数器
void Encoder_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H__ */
