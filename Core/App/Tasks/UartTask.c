#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "main.h"
#include "usart.h"

extern volatile float g_distance;           // HCSR04Task 写入的共享距离
extern osMessageQueueId_t TrackHandle;
extern volatile uint8_t g_obs_state;

static const char* obs_state_names[] = {
    "LINE_FOLLOW",
    "OBS_STOP",
    "TURN_LEFT",
    "CHK_LEFT",
    "TURN_RIGHT",
    "CHK_RIGHT",
    "BYPASS_FWD"
};

void StartUartTask(void *argument) {
    /* USER CODE BEGIN StartUartTask */
    char msg[128];
    float distance = 0.0f;
    uint8_t track_data = 0;
    HAL_StatusTypeDef tx_status;
    /* Infinite loop */
    for(;;)
    {
        distance = g_distance;              // 直接读全局
        osMessageQueueGet(TrackHandle, &track_data, 0, 10);

        uint8_t state = g_obs_state;
        if (state > 6) state = 0;

        int dist_int = (int)distance;
        int dist_dec = (int)((distance - dist_int) * 100);

        // UART 已移至 HCSR04.c 中输出，此处注释掉
        //int len = sprintf(msg, "ST:%s | dis:%d.%02dcm | track:0x%02X\r\n",
        //        obs_state_names[state], dist_int, dist_dec, track_data);
        //if (len > 0) {
        //    tx_status = HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)len, 200);
        //    if (tx_status != HAL_OK) {
        //        HAL_UART_DeInit(&huart2);
        //        MX_USART2_UART_Init();
        //    }
        //}

        osDelay(500);
    }
    /* USER CODE END StartUartTask */
}
