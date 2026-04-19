#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "main.h"
#include "usart.h"
extern osMessageQueueId_t LEDFlashHandle;

void StartLedTask(void *argument) {
    uint32_t flashCount = 0;
    uint8_t led_last;
    char msg[]="LED Flip Count:";
    char count_buf[20];
    led_last = HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin);
        /* Infinite loop */
    for(;;) {
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            // ????????
            uint8_t led_current = HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin);
            if(led_last == 0 && led_current == 1)
            {
                flashCount++;
                //HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);

                // 2. ??????flashCount???????????????
               // sprintf(count_buf, "%lu\r\n", flashCount);
                //HAL_UART_Transmit(&huart2, (uint8_t*)count_buf, strlen(count_buf), 100);

                osMessageQueuePut(LEDFlashHandle, &flashCount, 0, osWaitForever);
            }
            // ???????????
            led_last = led_current;


            osDelay(500);
            /* USER CODE END StartLedTask */
            }
    }//
    // Created by lirui on 2026/4/19.
    //
