#include "cmsis_os2.h"


extern osMessageQueueId_t DistanceHandle;
extern osMessageQueueId_t TrackHandle;
extern osMessageQueueId_t MotorActionHandle;

// 指令结构体定义
typedef enum {
    MOTOR_CMD_STOP = 0,      // 停止
    MOTOR_CMD_FORWARD,       // 直行
    MOTOR_CMD_TURN_LEFT,     // 左转
    MOTOR_CMD_TURN_RIGHT     // 右转
} MotorCmdTypeDef;

typedef struct {
    MotorCmdTypeDef cmd;     // 动作指令
    uint16_t pwm;            // 速度PWM值 0-1000
} MotorActionMsg;

void StartCtrlTask(void *argument) {
    float distance = 0.0f;
    uint8_t track_data = 0;   // 0-15 的四路循迹数据
    MotorActionMsg motor_cmd;

    // 初始化默认状态
    motor_cmd.cmd = MOTOR_CMD_STOP;
    motor_cmd.pwm = 600;     // 初始速度 60%

    for (;;) {
        // 1. 数据采集：非阻塞获取，保证任务不卡死
        osMessageQueueGet(DistanceHandle, &distance, 0, 10);
        osMessageQueueGet(TrackHandle, &track_data, 0, 10);

        // 2. 距离避障逻辑：最高优先级！
        // 当距离小于等于10cm时，无论如何都要立刻停止
        if (distance <= 10.0f) {
            motor_cmd.cmd = MOTOR_CMD_STOP;
            motor_cmd.pwm = 0;
        }
        // 3. 循迹逻辑：距离安全时，根据循迹数据走
        else {
            switch(track_data) {
                
                // 0000：完全偏离赛道 → 直行找回
                case 0x00:
                //0110 (L1+R1+R2 扩展)
                case 0x06:
                case 0x09:    //、1001 (L0+R2)
                    // 0101 (L1+R1)、
                    // 1010 (R1+R2)、
                case 0x05:
                case 0x0A:
                    motor_cmd.cmd = MOTOR_CMD_FORWARD;
                    motor_cmd.pwm = 550;
                    break;

                    // ---- 左侧有信号（黑线在左 → 向左修正） ----
                    // 0001 (L0 左后)
                case 0x01:
                    // 0010 (L1 左前)
                case 0x02:
                    // 0011 (L0+L1 左双)
                case 0x03:
                    //1101 (L0+L1+R2)、1110 (L0+L1+R1+R2 扩展)
                case 0x0D: case 0x0E:
                    motor_cmd.cmd = MOTOR_CMD_TURN_LEFT;  // 向左修正
                    motor_cmd.pwm = 400;                  // 修正速度，稳一点
                    break;

                    // ---- 右侧有信号（黑线在右 → 向右修正） ----
                    // 0100 (R1 右前)
                case 0x04:
                    // 1000 (R2 右后)
                case 0x08:
                    // 1100 (R1+R2 右双)
                case 0x0C:
                    //1011 (L0+R1+R2)、
                case 0x0B:
                    motor_cmd.cmd = MOTOR_CMD_TURN_RIGHT; // 向右修正
                    motor_cmd.pwm = 400;                  // 修正速度
                    break;

                    // 1111：全信号（压线/十字交叉）→ 减速直行
                case 0x0F:
                    motor_cmd.cmd = MOTOR_CMD_FORWARD;
                    motor_cmd.pwm = 550;
                    break;

                    // 默认：直行（安全兜底）
                default:
                    motor_cmd.cmd = MOTOR_CMD_FORWARD;
                    motor_cmd.pwm = 600;
                    break;
            }
        }

        // 4. 将指令发送到电机队列
        // 非阻塞发送，如果队列满了就等下一轮，保证控制频率
        osMessageQueuePut(MotorActionHandle, &motor_cmd, 0, 5);

        // 5. 任务延时：控制采样频率
        osDelay(30); // 30ms 一个循环，响应足够快
    }
}