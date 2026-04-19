#include <stdio.h>
#include "main.h"
#include "cmsis_os2.h"
#include "oled.h"
#include "FreeRTOS.h"


extern osMessageQueueId_t LEDFlashHandle;
extern osMessageQueueId_t DistanceHandle;
void StartOledTask(void *argument) {
     /* USER CODE BEGIN StartOledTask */
     uint32_t flashCount = 0;
     char str[20];
     float distance;
     char dismeg[32];
     OLED_Init();  // 添加这行初始化
     /* Infinite loop */
     for(;;)
     {
         osMessageQueueGet(LEDFlashHandle, &flashCount, 0, 0);
         osMessageQueueGet(DistanceHandle, &distance, 0, osWaitForever);
         int dist_int = distance;
         int dist_dec = (distance - dist_int)*100;
         sprintf(str, "flashCount: %lu", flashCount);
         sprintf(dismeg, "dis: %d.%02d cm", dist_int, dist_dec);
         OLED_NewFrame();
         OLED_PrintString(1, 1, "hello，world", &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 16, str, &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 32, dismeg, &font16x16, OLED_COLOR_NORMAL);
         OLED_ShowFrame();
         osDelay(10);

     }
     /* USER CODE END StartOledTask */
 }

//
// Created by lirui on 2026/4/19.
//
