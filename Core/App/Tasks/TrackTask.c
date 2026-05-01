#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "main.h"
#include "gpio.h"

extern osMessageQueueId_t TrackHandle;

uint8_t Track_Get_X1(void) {
    return HAL_GPIO_ReadPin(X1_GPIO_Port, X1_Pin) == GPIO_PIN_RESET ? 0 : 1;
}
uint8_t Track_Get_X2(void){
    return HAL_GPIO_ReadPin(X2_GPIO_Port, X2_Pin) == GPIO_PIN_RESET ? 0 : 1;
}

uint8_t Track_Get_X3(void){
    return HAL_GPIO_ReadPin(X3_GPIO_Port, X3_Pin) == GPIO_PIN_RESET ? 0 : 1;
}

uint8_t Track_Get_X4(void){
    return HAL_GPIO_ReadPin(X4_GPIO_Port, X4_Pin) == GPIO_PIN_RESET ? 0 : 1;
}


uint8_t Track_Get_AllStatus(void)
{
    uint8_t sta = 0;
    sta |= (Track_Get_X4() << 3);
    sta |= (Track_Get_X3() << 2);
    sta |= (Track_Get_X2() << 1);
    sta |= (Track_Get_X1() << 0);
    return sta;
}

void StartTrackTask(void *argument) {
    uint8_t status;

    for (;;)
    {
        status = Track_Get_AllStatus();

        // 发送到队列（CtrlTask/OledTask/UartTask 消费）
        osMessageQueuePut(TrackHandle, &status, 0, 0);

        osDelay(80);  // 80ms 周期（原来50ms太频繁，且去掉了串口输出避免冲突）
    }
}
