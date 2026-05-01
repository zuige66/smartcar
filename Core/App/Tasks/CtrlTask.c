/**
  ******************************************************************************
  * @file    CtrlTask.c
  * @brief   综合控制任务
  * @details 决策中枢：读取超声波和循迹传感器数据，使用PID控制电机
  *          包含避障状态机：检测障碍物后左转90°探测，不行再右转180°
  ******************************************************************************
  */

#include "cmsis_os2.h"
#include "pid.h"
#include "Encoder.h"

// 外部变量声明
extern volatile float g_distance;           // HCSR04Task 写入的共享距离
extern osMessageQueueId_t TrackHandle;
extern osMessageQueueId_t MotorActionHandle;

// 电机控制指令枚举
typedef enum {
    MOTOR_CMD_STOP = 0,      // 停止
    MOTOR_CMD_FORWARD,       // 前进
    MOTOR_CMD_TURN_LEFT,     // 差速左转（循迹用）
    MOTOR_CMD_TURN_RIGHT,    // 差速右转（循迹用）
    MOTOR_CMD_SPIN_LEFT,     // 原地左转（左轮后退+右轮前进）
    MOTOR_CMD_SPIN_RIGHT     // 原地右转（左轮前进+右轮后退）
} MotorCmdTypeDef;

// 电机控制指令结构体
typedef struct {
    MotorCmdTypeDef cmd;     // 动作指令
    uint16_t pwm;            // 速度PWM值 0-1000（避障用）
    uint16_t pwm_left;       // 左轮PWM（循迹差速用）
    uint16_t pwm_right;      // 右轮PWM（循迹差速用）
} MotorActionMsg;

// 避障状态机
typedef enum {
    STATE_LINE_FOLLOW = 0,   // 正常循迹
    STATE_OBS_STOP,          // 检测到障碍，停车
    STATE_OBS_TURN_LEFT,     // 向左转90°
    STATE_OBS_CHECK_LEFT,    // 检查左侧是否有路
    STATE_OBS_TURN_RIGHT,    // 左侧不通，向右转180°
    STATE_OBS_CHECK_RIGHT,   // 检查右侧是否有路
    STATE_OBS_FORWARD_BYPASS // 找到路，前进绕过障碍
} ObstacleState;

// PID控制器实例
static PID_HandleTypeDef speed_pid;   // 速度PID
static PID_HandleTypeDef turn_pid;    // 转向PID

// 当前避障状态（供OLED/串口显示）
volatile uint8_t g_obs_state = 0;

// 目标速度
#define TARGET_SPEED 300.0f  // 目标速度（编码器脉冲数）

// 避障参数
#define OBS_DETECT_DIST     20.0f   // 障碍物检测距离(cm)
#define OBS_TURN_PWM        700     // 避障转弯PWM（原地转弯需要更大功率）
#define OBS_FORWARD_PWM     250     // 避障前进PWM

// 避障时序（控制周期30ms）
#define OBS_STOP_CYCLES     7       // 停车稳定 ~200ms
#define OBS_TURN_90_CYCLES  27      // 90°转弯 ~800ms（需根据实际调整）
#define OBS_TURN_180_CYCLES 53      // 180°转弯 ~1600ms
#define OBS_BYPASS_CYCLES   33      // 绕行前进 ~1000ms
#define OBS_CHECK_WAIT      3       // 检查等待 ~90ms（等最新距离数据）
#define OBS_TOTAL_TIMEOUT   333     // 整体超时 ~10s（防止卡死）

/**
  * @brief  计算循迹偏差
  * @param  track_data: 四路循迹传感器状态 (0x00-0x0F)
  * @retval 偏差值：负数=偏左，正数=偏右，0=居中
  */
static int8_t CalculateTrackError(uint8_t track_data) {
    switch(track_data) {
        // 居中状态
        case 0x06:  // 0110 - L1+R1
        case 0x09:  // 1001 - L0+R2
        case 0x05:  // 0101 - L1+R1
        case 0x0A:  // 1010 - R1+R2
            return 0;

        // 稍微偏左
        case 0x02:  // 0010 - L1
            return -1;

        // 偏左
        case 0x01:  // 0001 - L0
        case 0x03:  // 0011 - L0+L1
            return -2;

        // 严重偏左
        case 0x0D:  // 1101 - L0+L1+R2
        case 0x0E:  // 1110 - L0+L1+R1+R2
            return -3;

        // 稍微偏右
        case 0x04:  // 0100 - R1
            return 1;

        // 偏右
        case 0x08:  // 1000 - R2
        case 0x0C:  // 1100 - R1+R2
            return 2;

        // 严重偏右
        case 0x0B:  // 1011 - L0+R1+R2
            return 3;

        // 全检测到或全未检测到
        case 0x00:  // 0000 - 全未检测到
        case 0x0F:  // 1111 - 全检测到
        default:
            return 0;
    }
}

/**
  * @brief  综合控制任务
  * @param  argument: 任务参数（未使用）
  * @details 读取传感器数据，使用PID控制电机，避障状态机
  */
void StartCtrlTask(void *argument) {
    (void) argument;

    float distance = 0.0f;
    uint8_t track_data = 0;
    MotorActionMsg motor_cmd;
    int32_t encoder_speed = 0;
    int8_t track_error = 0;

    // 避障状态机变量
    ObstacleState obs_state = STATE_LINE_FOLLOW;
    uint32_t state_counter = 0;   // 状态内计时（30ms/次）
    uint32_t obs_total_counter = 0; // 避障总耗时计数

    // 初始化PID控制器
    PID_Init(&speed_pid, 2.0f, 0.1f, 0.5f);  // 速度PID: Kp, Ki, Kd
    PID_SetTarget(&speed_pid, TARGET_SPEED);
    PID_SetOutputLimits(&speed_pid, 0, 600);

    PID_Init(&turn_pid, 80.0f, 0.0f, 20.0f);  // 转向PID: Kp, Ki, Kd
    PID_SetTarget(&turn_pid, 0);  // 目标：偏差为0（居中）
    PID_SetOutputLimits(&turn_pid, -250, 250);

    // 初始化编码器
    Encoder_Init();

    // 初始状态：停止
    motor_cmd.cmd = MOTOR_CMD_STOP;
    motor_cmd.pwm = 0;
    motor_cmd.pwm_left = 0;
    motor_cmd.pwm_right = 0;

    for (;;) {
        // 1. 读取传感器数据
        distance = g_distance;              // 直接读全局（HCSR04Task 写入）
        osMessageQueueGet(TrackHandle, &track_data, 0, 10);

        // 2. 读取编码器速度并重置
        encoder_speed = Encoder_GetSpeed();
        Encoder_Reset();

        // 3. 计算循迹偏差
        track_error = CalculateTrackError(track_data);

        // 4. 避障状态机
        // 超时保护：避障超过10秒强制回到循迹
        if (obs_state != STATE_LINE_FOLLOW) {
            obs_total_counter++;
            if (obs_total_counter >= OBS_TOTAL_TIMEOUT) {
                obs_state = STATE_LINE_FOLLOW;
                state_counter = 0;
                obs_total_counter = 0;
                PID_Reset(&speed_pid);
                PID_Reset(&turn_pid);
                motor_cmd.cmd = MOTOR_CMD_STOP;
                motor_cmd.pwm = 0;
            }
        }

        switch (obs_state) {

        // ==================== 正常循迹（PID平滑差速） ====================
        case STATE_LINE_FOLLOW:
            obs_total_counter = 0;
            // distance > 0 且 <= 20cm 时触发避障（0表示传感器无数据）
            if (distance > 0 && distance <= OBS_DETECT_DIST) {
                obs_state = STATE_OBS_STOP;
                state_counter = 0;
                PID_Reset(&speed_pid);
                PID_Reset(&turn_pid);
                motor_cmd.cmd = MOTOR_CMD_STOP;
                motor_cmd.pwm = 0;
                motor_cmd.pwm_left = 0;
                motor_cmd.pwm_right = 0;
                break;
            }
            // PID平滑差速循迹
            {
                float speed_output = PID_Compute(&speed_pid, (float)encoder_speed);
                float turn_output = PID_Compute(&turn_pid, (float)track_error);

                // 速度PID输出作为基准PWM
                float base_pwm = speed_output;
                if (base_pwm < 50) base_pwm = 50;

                // 转向PID输出做差速修正
                // turn_output > 0 → 偏左 → 右轮加速、左轮减速（向右修正）
                // turn_output < 0 → 偏右 → 左轮加速、右轮减速（向左修正）
                int32_t left_pwm  = (int32_t)(base_pwm + turn_output);
                int32_t right_pwm = (int32_t)(base_pwm - turn_output);

                // 限幅
                if (left_pwm < 50) left_pwm = 50;
                if (left_pwm > 600) left_pwm = 600;
                if (right_pwm < 50) right_pwm = 50;
                if (right_pwm > 600) right_pwm = 600;

                motor_cmd.cmd = MOTOR_CMD_FORWARD;
                motor_cmd.pwm_left = (uint16_t)left_pwm;
                motor_cmd.pwm_right = (uint16_t)right_pwm;
            }
            break;

        // ==================== 障碍停车 ====================
        case STATE_OBS_STOP:
            motor_cmd.cmd = MOTOR_CMD_STOP;
            motor_cmd.pwm = 0;
            state_counter++;
            if (state_counter >= OBS_STOP_CYCLES) {
                obs_state = STATE_OBS_TURN_LEFT;
                state_counter = 0;
            }
            break;

        // ==================== 原地左转90° ====================
        case STATE_OBS_TURN_LEFT:
            motor_cmd.cmd = MOTOR_CMD_SPIN_LEFT;
            motor_cmd.pwm = OBS_TURN_PWM;
            state_counter++;
            if (state_counter >= OBS_TURN_90_CYCLES) {
                obs_state = STATE_OBS_CHECK_LEFT;
                state_counter = 0;
                motor_cmd.cmd = MOTOR_CMD_STOP;
                motor_cmd.pwm = 0;
            }
            break;

        // ==================== 检查左侧（等待最新数据） ====================
        case STATE_OBS_CHECK_LEFT:
            motor_cmd.cmd = MOTOR_CMD_STOP;
            motor_cmd.pwm = 0;
            state_counter++;
            if (state_counter >= OBS_CHECK_WAIT) {
                // 等待完成后检查距离
                if (distance > OBS_DETECT_DIST) {
                    obs_state = STATE_OBS_FORWARD_BYPASS;
                } else {
                    obs_state = STATE_OBS_TURN_RIGHT;
                }
                state_counter = 0;
            }
            break;

        // ==================== 原地右转180° ====================
        case STATE_OBS_TURN_RIGHT:
            motor_cmd.cmd = MOTOR_CMD_SPIN_RIGHT;
            motor_cmd.pwm = OBS_TURN_PWM;
            state_counter++;
            if (state_counter >= OBS_TURN_180_CYCLES) {
                obs_state = STATE_OBS_CHECK_RIGHT;
                state_counter = 0;
                motor_cmd.cmd = MOTOR_CMD_STOP;
                motor_cmd.pwm = 0;
            }
            break;

        // ==================== 检查右侧（等待最新数据） ====================
        case STATE_OBS_CHECK_RIGHT:
            motor_cmd.cmd = MOTOR_CMD_STOP;
            motor_cmd.pwm = 0;
            state_counter++;
            if (state_counter >= OBS_CHECK_WAIT) {
                if (distance > OBS_DETECT_DIST) {
                    obs_state = STATE_OBS_FORWARD_BYPASS;
                } else {
                    // 死胡同，回到循迹
                    obs_state = STATE_LINE_FOLLOW;
                }
                state_counter = 0;
            }
            break;

        // ==================== 绕行前进 ====================
        case STATE_OBS_FORWARD_BYPASS:
            motor_cmd.cmd = MOTOR_CMD_FORWARD;
            motor_cmd.pwm = OBS_FORWARD_PWM;
            state_counter++;
            // 前进过程中如果又检测到障碍，重新进入避障
            if (distance > 0 && distance <= OBS_DETECT_DIST) {
                obs_state = STATE_OBS_STOP;
                state_counter = 0;
                break;
            }
            if (state_counter >= OBS_BYPASS_CYCLES) {
                // 绕行完成，回到循迹
                obs_state = STATE_LINE_FOLLOW;
                state_counter = 0;
                PID_Reset(&speed_pid);
                PID_Reset(&turn_pid);
            }
            break;
        }

        // 5. 更新全局状态（供显示）
        g_obs_state = (uint8_t)obs_state;

        // 6. 发送指令到电机驱动任务
        osMessageQueuePut(MotorActionHandle, &motor_cmd, 0, 5);

        // 7. 控制周期：30ms
        osDelay(30);
    }
}
