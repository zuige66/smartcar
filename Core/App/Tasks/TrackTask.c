#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "main.h"
#include "usart.h"
#include "gpio.h"
extern osThreadId_t TrackTaskHandle;
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
    uint8_t x1, x2, x3, x4, status;
    uint8_t msg[64];

    for (;;)
    {
        x1 = Track_Get_X1();
        x2 = Track_Get_X2();
        x3 = Track_Get_X3();
        x4 = Track_Get_X4();
        status = Track_Get_AllStatus();

        sprintf((char*)msg, "X4:%d X3:%d X2:%d X1:%d | STA:0x%02X\r\n", x4, x3, x2, x1, status);
        HAL_UART_Transmit(&huart2, msg, strlen((char*)msg), 100);

        osMessageQueuePut(TrackHandle, &status, 0, 0);
        osDelay(500);
    }
}