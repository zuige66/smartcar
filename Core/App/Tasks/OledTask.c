#include <stdio.h>
#include "main.h"
#include "cmsis_os2.h"
#include "oled.h"
#include "FreeRTOS.h"


extern osMessageQueueId_t LEDFlashHandle;
extern osMessageQueueId_t DistanceHandle;
extern osMessageQueueId_t TrackHandle;
extern osMessageQueueId_t DriverPWMHandle;
void StartOledTask(void *argument) {
     /* USER CODE BEGIN StartOledTask */
     uint32_t flashCount = 0;
     char str[20];
     float distance;
     char dismeg[20];
     uint8_t  status;
     char status_str[20];
     char status_div_str[20];
     uint8_t x1, x2, x3, x4;
     uint32_t msg;
     char msg_str[20];
     OLED_Init();  // 添加这行初始化
     /* Infinite loop */
     for(;;)
     {
         osMessageQueueGet(LEDFlashHandle, &flashCount, 0, 0);
         osMessageQueueGet(DistanceHandle, &distance, 0, 0);
         osMessageQueueGet(TrackHandle, &status, 0, 0);
         osMessageQueueGet(DriverPWMHandle, &msg, 0, 0);
         int dist_int = distance;
         int dist_dec = (distance - dist_int)*100;
         x1 = (status >> 0) & 1;
         x2 = (status >> 1) & 1;
         x3 = (status >> 2) & 1;
         x4 = (status >> 3) & 1;
         sprintf(str, "flashCount: %lu", flashCount);

         sprintf(dismeg, "dis: %d.%02d cm", dist_int, dist_dec);
         sprintf(status_str, "track:%d  ",  status);
         sprintf(status_div_str, "X1X2X3X4:%d%d%d%d",  x1, x2,x3, x4);
         sprintf(msg_str, "%d", msg);
         OLED_NewFrame();
         //OLED_PrintString(1, 1, "hello，world", &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 1, str, &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 16, dismeg, &font16x16, OLED_COLOR_NORMAL);
         //OLED_PrintString(1, 32, status_str, &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 32, msg_str, &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 48, status_div_str, &font16x16, OLED_COLOR_NORMAL);
         OLED_ShowFrame();
         osDelay(10);

     }
     /* USER CODE END StartOledTask */
 }

//
// Created by lirui on 2026/4/19.
//
