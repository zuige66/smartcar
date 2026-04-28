#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "main.h"
#include "usart.h"
extern osMessageQueueId_t DistanceHandle;

void StartUartTask(void *argument) {
    /* USER CODE BEGIN StartUartTask */
    char msg[64];
    float distance;
    osStatus_t status;
    /* Infinite loop */
    for(;;)
    {
        // // 发送距离数据
        // status = osMessageQueueGet(DistanceHandle, &distance, 0, 1000);
        // if (status == osOK) {
        //     sprintf(msg, "Distance: %.2f cm \r\n", distance);
        // } else {
        //     sprintf(msg, "Distance: error (status: %d)\r\n", status);
        // }
        // HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
        
        osDelay(500);
    }
    /* USER CODE END StartUartTask */
}
