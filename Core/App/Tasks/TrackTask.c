/**
  ******************************************************************************
  * @file    TrackTask.c
  * @brief   循迹传感器任务
  * @details 周期性读取四路循迹传感器状态，将数据发送到消息队列
  *          传感器布局：X1(最左) X2(左) X3(右) X4(最右)
  *          返回值：低4位表示4个传感器状态，0=检测到黑线，1=未检测到
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "main.h"
#include "gpio.h"

/** @brief 循迹数据消息队列句柄 */
extern osMessageQueueId_t TrackHandle;

/**
  * @brief  读取X1传感器状态（最左侧）
  * @retval 0=检测到黑线，1=未检测到（高电平）
  */
uint8_t Track_Get_X1(void) {
    return HAL_GPIO_ReadPin(X1_GPIO_Port, X1_Pin) == GPIO_PIN_RESET ? 0 : 1;
}

/**
  * @brief  读取X2传感器状态（左侧）
  * @retval 0=检测到黑线，1=未检测到（高电平）
  */
uint8_t Track_Get_X2(void){
    return HAL_GPIO_ReadPin(X2_GPIO_Port, X2_Pin) == GPIO_PIN_RESET ? 0 : 1;
}

/**
  * @brief  读取X3传感器状态（右侧）
  * @retval 0=检测到黑线，1=未检测到（高电平）
  */
uint8_t Track_Get_X3(void){
    return HAL_GPIO_ReadPin(X3_GPIO_Port, X3_Pin) == GPIO_PIN_RESET ? 0 : 1;
}

/**
  * @brief  读取X4传感器状态（最右侧）
  * @retval 0=检测到黑线，1=未检测到（高电平）
  */
uint8_t Track_Get_X4(void){
    return HAL_GPIO_ReadPin(X4_GPIO_Port, X4_Pin) == GPIO_PIN_RESET ? 0 : 1;
}

/**
  * @brief  获取四路传感器综合状态
  * @retval 8位状态值：bit3=X4, bit2=X3, bit1=X2, bit0=X1
  *         例如：0x06 = 0b0110 = X2+X3检测到黑线（居中）
  */
uint8_t Track_Get_AllStatus(void)
{
    uint8_t sta = 0;
    sta |= (Track_Get_X4() << 3);  // 最右
    sta |= (Track_Get_X3() << 2);  // 右
    sta |= (Track_Get_X2() << 1);  // 左
    sta |= (Track_Get_X1() << 0);  // 最左
    return sta;
}

/**
  * @brief  循迹任务入口函数
  * @param  argument: 任务参数（未使用）
  * @details 以80ms周期读取四路循迹传感器，将状态发送到消息队列
  */
void StartTrackTask(void *argument) {
    (void) argument;
    uint8_t status;

    for (;;)
    {
        // 读取四路传感器综合状态
        status = Track_Get_AllStatus();

        // 发送到队列（CtrlTask/OledTask/UartTask 订阅）
        osMessageQueuePut(TrackHandle, &status, 0, 0);

        // 采样周期：80ms
        // 注意：原来50ms太频繁，且去掉了串口输出避免UART冲突
        osDelay(80);
    }
}