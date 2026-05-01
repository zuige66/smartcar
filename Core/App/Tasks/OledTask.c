#include <stdio.h>
#include "main.h"
#include "cmsis_os2.h"
#include "oled.h"

extern osMessageQueueId_t DistanceHandle;
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

void StartOledTask(void *argument) {
     /* USER CODE BEGIN StartOledTask */
     float distance = 0.0f;
     char line0[22];
     char line1[22];
     char line2[22];
     char line3[22];
     uint8_t status = 0;
     uint8_t x1, x2, x3, x4;
     OLED_Init();
     /* Infinite loop */
     for(;;)
     {
         osMessageQueueGet(DistanceHandle, &distance, 0, 10);
         osMessageQueueGet(TrackHandle, &status, 0, 10);

         int dist_int = (int)distance;
         int dist_dec = (int)((distance - dist_int) * 100);
         x1 = (status >> 0) & 1;
         x2 = (status >> 1) & 1;
         x3 = (status >> 2) & 1;
         x4 = (status >> 3) & 1;

         uint8_t state = g_obs_state;
         if (state > 6) state = 0;

         sprintf(line0, "ST:%s", obs_state_names[state]);
         sprintf(line1, "dis:%d.%02dcm", dist_int, dist_dec);
         sprintf(line2, "track:0x%02X", status);
         sprintf(line3, "X1X2X3X4:%d%d%d%d", x1, x2, x3, x4);

         OLED_NewFrame();
         OLED_PrintString(1, 1, line0, &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 16, line1, &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 32, line2, &font16x16, OLED_COLOR_NORMAL);
         OLED_PrintString(1, 48, line3, &font16x16, OLED_COLOR_NORMAL);
         OLED_ShowFrame();
         osDelay(300);
     }
     /* USER CODE END StartOledTask */
 }

//
// Created by lirui on 2026/4/19.
//
