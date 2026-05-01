# STM32F103C8T6 FreeRTOS 智能小车 - AI 项目档案

> **目标 AI**: 本文档专为 LLM 代码助手优化，无需逐个阅读源文件即可理解完整项目上下文。
> **最后更新**: 2026-05-01

---

## 1. 项目概述

- **MCU**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, **20KB RAM**)
- **RTOS**: FreeRTOS v10.3.1，通过 CMSIS-RTOS v2 封装层使用
- **IDE**: CLion + STM32CubeMX（代码生成）+ OpenOCD（烧录/调试）
- **功能**: 循迹智能小车，具备避障、超声波测距、OLED 显示、串口调试功能
- **语言**: C (C99)

### 1.1 功能列表

1. 4 路红外循迹传感器（PID 平滑差速转向）
2. HC-SR04 超声波障碍物检测（轮询模式）
3. 7 状态避障状态机（停车 → 原地左转 90° → 检测 → 原地右转 180° → 检测 → 绕行）
4. SSD1306 128x64 OLED 实时状态显示
5. UART 115200 调试输出
6. TIM3 正交编码器速度反馈
7. L298N 双电机 PWM 调速驱动

---

## 2. 硬件引脚映射

| 功能 | 引脚 | 定时器/外设 | 备注 |
|---|---|---|---|
| 左电机 AIN1 | PA4 | GPIO | 方向控制 |
| 左电机 AIN2 | PA5 | GPIO | 方向控制 |
| 右电机 BIN1 | PA6 | GPIO | 方向控制 |
| 右电机 BIN2 | PA7 | GPIO | 方向控制 |
| 左电机 PWM | PA0 | TIM2_CH1 | 速度控制, 0-1000 |
| 右电机 PWM | PA1 | TIM2_CH2 | 速度控制, 0-1000 |
| 编码器左 CH1 | PB4 | TIM3_CH1 | 部分重映射 |
| 编码器左 CH2 | PB5 | TIM3_CH2 | 部分重映射 |
| HC-SR04 Echo | PA10 | TIM1_CH3 / GPIO | 轮询模式下作为 GPIO 输入 |
| HC-SR04 Trig | PA11 | GPIO | 输出触发脉冲 |
| 循迹传感器 X1 | PB14 | GPIO | 红外传感器输入 |
| 循迹传感器 X2 | PB15 | GPIO | 红外传感器输入 |
| 循迹传感器 X3 | PA8 | GPIO | 红外传感器输入 |
| 循迹传感器 X4 | PA9 | GPIO | 红外传感器输入 |
| LED | PA12 | GPIO | 状态指示灯 |
| I2C SCL | PB6 | I2C1 | OLED 时钟线 |
| I2C SDA | PB7 | I2C1 | OLED 数据线 |
| UART TX | PA2 | USART2 | 调试输出 |
| UART RX | PA3 | USART2 | 调试输入 |

---

## 3. CubeMX 配置摘要

### 3.1 时钟配置

- HSE 8MHz 外部晶振
- PLL: 8MHz × 9 = 72MHz (SYSCLK)
- AHB = 72MHz, APB1 = 36MHz, APB2 = 72MHz

### 3.2 TIM1（超声波定时）

- 预分频器: 71 → 1MHz 计数器（1 tick = 1µs）
- 周期: 65535
- CH3 输入捕获，PA10（轮询模式下未使用输入捕获功能）
- NVIC: TIM1_CC_IRQn 优先级 = 5

### 3.3 TIM2（电机 PWM）

- 预分频器: 0 → 72MHz
- 周期: 999 → PWM 频率 = 72kHz
- CH1 (PA0): 左电机 PWM
- CH2 (PA1): 右电机 PWM
- PWM 模式 1，快速 PWM

### 3.4 TIM3（编码器）

- 双通道编码器模式
- 周期: 65535
- 部分重映射: CH1=PB4, CH2=PB5
- 无预分频，每个边沿都计数

### 3.5 I2C1（OLED）

- 标准模式, 100kHz
- PB6 (SCL), PB7 (SDA)

### 3.6 USART2（调试串口）

- 波特率 115200, 8N1
- PA2 (TX), PA3 (RX)
- 无硬件流控

### 3.7 GPIO 配置

| 引脚 | 模式 | 上下拉 | 速度 |
|---|---|---|---|
| PA4-PA7（电机方向） | 推挽输出 | 无 | 低速 |
| PA8, PA9, PB14, PB15（循迹） | 输入 | 无（浮空） | - |
| PA10（Echo） | 输入 | 无（浮空） | - |
| PA11（Trig） | 推挽输出 | 无 | 低速 |
| PA12（LED） | 推挽输出 | 无 | 低速 |

---

## 4. FreeRTOS 任务架构

### 4.1 任务列表（按优先级降序排列）

| 任务 | 优先级 (CMSIS) | CubeMX 值 | 栈大小 | 周期 | 文件 |
|---|---|---|---|---|---|
| HCSR04Task | osPriorityRealtime (48) | 48 | 512B | ~100ms | HCSR04.c |
| TrackTask | osPriorityAboveNormal (40) | 40 | 512B | 80ms | TrackTask.c |
| DriverTask | osPriorityNormal (24) | 24 | 512B | 事件驱动 | DriverTask.c |
| CtrlTask | osPriorityNormal (24) | 24 | 512B | 30ms | CtrlTask.c |
| OledTask | osPriorityLow (12) | 12 | 640B | 300ms | OledTask.c |
| UartTask | osPriorityLow1 (13) | 13 | 640B | 500ms | UartTask.c |
| LedTask | osPriorityLow (12) | 12 | 512B | 500ms | LedTask.c |

### 4.2 数据流

```
HCSR04Task ──[g_distance 全局变量]──→ CtrlTask ──[MotorActionQueue]──→ DriverTask ──→ 电机
TrackTask  ──[TrackQueue]──────────→ CtrlTask                                   ↑
HCSR04Task ──[g_distance 全局变量]──→ OledTask                            TIM3 编码器（反馈）
TrackTask  ──[TrackQueue]──────────→ OledTask
HCSR04Task ──[g_distance 全局变量]──→ UartTask
TrackTask  ──[TrackQueue]──────────→ UartTask
```

### 4.3 共享数据（全局变量）

| 变量 | 类型 | 写入者 | 读取者 | 备注 |
|---|---|---|---|---|
| g_distance | volatile float | HCSR04Task | CtrlTask, OledTask, UartTask | 距离值(cm), 0.0=无效 |
| g_obs_state | volatile uint8_t | CtrlTask | OledTask, UartTask | 避障状态机当前状态 |

### 4.4 队列定义

| 队列 | 元素大小 | 深度 | 生产者 | 消费者 |
|---|---|---|---|---|
| TrackHandle | sizeof(uint8_t) = 1B | 16 | TrackTask | CtrlTask, OledTask, UartTask |
| MotorActionHandle | 12B (MotorActionMsg) | 16 | CtrlTask | DriverTask |
| DistanceHandle | sizeof(float) = 4B | 16 | （未使用，遗留） | （未使用，遗留） |
| LEDFlashHandle | sizeof(uint32_t) = 4B | 16 | LedTask | （无消费者） |
| DriverPWMHandle | sizeof(uint32_t) = 4B | 16 | （无生产者） | （无消费者） |

### 4.5 信号量

| 信号量 | 类型 | 状态 |
|---|---|---|
| EchoBinarySem | 二值信号量 | **未使用**（已创建但从未获取/释放，原为中断式 HCSR04 预留） |

### 4.6 任务调度分析

| 任务 | 优先级 | 周期 | 每次执行 CPU 时间 | 最大 CPU 占用 | 饥饿风险 |
|---|---|---|---|---|---|
| HCSR04Task | 48 (Realtime) | ~100ms | 30-60ms（轮询）+ 3ms（触发） | ~60% | 无（最高优先级） |
| TrackTask | 40 (AboveNormal) | 80ms | <1ms | ~1% | 无 |
| DriverTask | 24 (Normal) | 事件驱动 | <1ms | ~1% | 无 |
| CtrlTask | 24 (Normal) | 30ms | 2-5ms | ~10% | 有：HCSR04 轮询期间被阻塞 30-60ms |
| OledTask | 12 (Low) | 300ms | 10-20ms（I2C） | ~5% | 有：被高优先级任务延迟 |
| UartTask | 13 (Low1) | 500ms | 2-5ms | ~1% | 有：被高优先级任务延迟 |
| LedTask | 12 (Low) | 500ms | <1ms | ~0.2% | 无 |

**关键说明**: HCSR04Task 轮询每约 100ms 阻塞 30-60ms。在此窗口期间，所有低优先级任务被抢占。这是可接受的，因为：
- CtrlTask 的 30ms 周期可能延迟最多 60ms → 总计约 90ms，电机控制可容忍
- OledTask/UartTask/LedTask 无实时要求
- TrackTask（优先级 40）每次执行不到 1ms，可在 HCSR04 触发和轮询之间穿插执行

---

## 5. 各模块详细说明

### 5.1 HC-SR04 超声波测距 (HCSR04.c)

**模式**: 轮询（非中断）

**输出**: 写入全局变量 `volatile float g_distance`（非队列）

**测量周期**（约 100ms）：
1. **预等待**: 等待 Echo 引脚变低（排除上次测量残留），超时 50ms
2. Trig 先拉低 1ms → 拉高 1ms → 拉低 1ms（产生 10µs 以上脉冲）
3. 将 TIM1 计数器清零
4. 轮询 Echo 引脚，等待高电平（上升沿），记录 `rising` 计数值
5. 轮询 Echo 引脚，等待低电平（下降沿），记录 `falling` 计数值
6. 持续时间 = falling - rising，合理性检查 117-23529us (2-400cm)
7. 距离 = 持续时间 × 0.017 cm
8. 写入 `g_distance`
9. osDelay(80)

**超时**: 每个边沿等待 30ms 超时，超时后跳转 sensor_fail（写入 g_distance=0.0f）

**关键依赖**: 任务启动时调用一次 `HAL_TIM_Base_Start(&htim1)`

### 5.2 循迹传感器 (TrackTask.c)

**传感器**: 4 路数字红外 (X1=PB14, X2=PB15, X3=PA8, X4=PA9)
- GPIO_PIN_SET = 检测到黑线（白底黑线）
- GPIO_PIN_RESET = 未检测到线

**状态字节**: `(X4<<3) | (X3<<2) | (X2<<1) | (X1<<0)`

**周期**: 80ms（原为 50ms，已减小以匹配传感器响应时间）

**输出**: 仅写入 TrackHandle 队列（UART 输出已移除，由 UartTask 统一处理）

### 5.3 控制任务 (CtrlTask.c)

**核心决策循环**，周期 30ms：

1. 读取 `g_distance` 全局变量 + 循迹队列（非阻塞）
2. 读取编码器速度，然后重置计数器
3. 通过 `CalculateTrackError()` 计算循迹偏差
4. 运行避障状态机
5. PID 计算：速度 PID → 基准 PWM，转向 PID → 差速修正量
6. 发送 MotorActionMsg 到 DriverTask

**MotorActionMsg 结构体**（12 字节）：
```c
typedef struct {
    MotorCmdTypeDef cmd;     // 枚举: STOP=0, FORWARD=1, TURN_LEFT=2, TURN_RIGHT=3, SPIN_LEFT=4, SPIN_RIGHT=5
    uint16_t pwm;            // 避障用 PWM (0-1000)
    uint16_t pwm_left;       // 循迹用左轮 PWM (0-1000)
    uint16_t pwm_right;      // 循迹用右轮 PWM (0-1000)
} MotorActionMsg;
```

**PID 参数**：
- 速度 PID: Kp=2.0, Ki=0.1, Kd=0.5, 目标值=300 编码器脉冲, 输出范围=[0,600]
- 转向 PID: Kp=80.0, Ki=0.0, Kd=20.0, 目标值=0, 输出范围=[-250,250]

**循迹偏差计算**（4 位状态 → 偏差 -3 到 +3）：

| 十六进制 | 二进制 | 偏差 | 含义 |
|---|---|---|---|
| 0x06 | 0110 | 0 | 居中 |
| 0x09 | 1001 | 0 | 居中 |
| 0x05 | 0101 | 0 | 居中 |
| 0x0A | 1010 | 0 | 居中 |
| 0x02 | 0010 | -1 | 稍偏左 |
| 0x01 | 0001 | -2 | 偏左 |
| 0x03 | 0011 | -2 | 偏左 |
| 0x0D | 1101 | -3 | 严重偏左 |
| 0x0E | 1110 | -3 | 严重偏左 |
| 0x04 | 0100 | +1 | 稍偏右 |
| 0x08 | 1000 | +2 | 偏右 |
| 0x0C | 1100 | +2 | 偏右 |
| 0x0B | 1011 | +3 | 严重偏右 |
| 0x00 | 0000 | 0 | 丢失（全未检测） |
| 0x0F | 1111 | 0 | 交叉线（全检测） |

**PID 平滑差速转向**（循迹模式）：
```
base_pwm = 速度PID输出 (最小 50)
turn_output = 转向PID输出 (-250 到 +250)
left_pwm  = base_pwm + turn_output  (限幅 50-600)
right_pwm = base_pwm - turn_output  (限幅 50-600)
```

### 5.4 避障状态机

```
LINE_FOLLOW ──(距离≤20cm)──→ OBS_STOP (7 周期 = 210ms)
     ↑                                │
     │                                ↓
     │                          OBS_TURN_LEFT (27 周期 = 810ms, 原地左转)
     │                                │
     │                                ↓
     │                          OBS_CHECK_LEFT (3 周期 = 90ms, 读取距离)
     │                             ╱           ╲
     │                    距离>20cm             距离≤20cm
     │                      ↓                       ↓
     │              OBS_FORWARD_BYPASS        OBS_TURN_RIGHT (53 周期 = 1590ms, 原地右转)
     │              (33 周期 = 990ms)               │
     │                      │                       ↓
     │                      │               OBS_CHECK_RIGHT (3 周期)
     │                      │                  ╱           ╲
     │                 完成/障碍           距离>20cm     距离≤20cm
     │                      ↓               ↓               ↓
     └──────────────────────┘          OBS_FORWARD      LINE_FOLLOW (死胡同，放弃)
                                             │
                                             ↓
                                       OBS_FORWARD_BYPASS
```

**避障参数**：

| 参数 | 值 | 说明 |
|---|---|---|
| OBS_DETECT_DIST | 20.0f cm | 触发避障的距离阈值 |
| OBS_TURN_PWM | 700 | 原地转弯 PWM 功率 |
| OBS_FORWARD_PWM | 250 | 绕行前进 PWM 功率 |
| OBS_STOP_CYCLES | 7 (210ms) | 停车稳定时间 |
| OBS_TURN_90_CYCLES | 27 (810ms) | 90° 原地旋转时间 |
| OBS_TURN_180_CYCLES | 53 (1590ms) | 180° 原地旋转时间 |
| OBS_BYPASS_CYCLES | 33 (990ms) | 绕行前进时间 |
| OBS_CHECK_WAIT | 3 (90ms) | 等待最新距离数据 |
| OBS_TOTAL_TIMEOUT | 333 (10s) | 整体超时，防止卡死 |

**超时保护**: 避障流程超过 10 秒则强制返回循迹模式，并重置 PID。

### 5.5 电机驱动 (motor.c)

**L298N 风格控制**：
- 前进: AIN1=高, AIN2=低 (及 BIN1=高, BIN2=低)
- 后退: AIN1=低, AIN2=高 (及 BIN1=低, BIN2=高)
- 停止: AIN1=高, AIN2=高（双高 = 制动）
- PWM: TIM2 CH1（左轮）, TIM2 CH2（右轮），范围 0-1000

**Motor_SetSpeed**: `pwm_val = (speed * PWM_MAX_VALUE) / 1000` → 写入比较寄存器

**DriverTask 指令执行**：

| 指令 | 左轮方向 | 右轮方向 | 左轮 PWM | 右轮 PWM |
|---|---|---|---|---|
| STOP | - | - | 0 | 0 |
| FORWARD | 前进 | 前进 | pwm_left | pwm_right |
| TURN_LEFT | 前进 | 前进 | pwm/3 | pwm |
| TURN_RIGHT | 前进 | 前进 | pwm | pwm/3 |
| SPIN_LEFT | **后退** | 前进 | pwm | pwm |
| SPIN_RIGHT | 前进 | **后退** | pwm | pwm |

### 5.6 编码器 (Encoder.c)

- TIM3 编码器模式，双通道双边沿计数
- PB4/PB5（部分重映射）
- `Encoder_GetSpeed()`: 返回 int16_t 计数值（有符号，负值表示反转）
- `Encoder_Reset()`: 计数器清零
- CtrlTask 每 30ms 调用一次

### 5.7 OLED 显示 (OledTask.c)

- SSD1306 128x64，通过 I2C 通信（地址 0x78）
- 1024 字节帧缓冲: `OLED_GRAM[8][128]`
- font16x16 字体（16px 高，4 行 = 64px 满屏）
- 显示内容: 避障状态、距离、循迹十六进制、X1X2X3X4 位状态
- 刷新率: 300ms
- I2C 超时: 100ms（之前使用 MAX_DELAY 导致阻塞）
- 错误恢复: I2C 总线失败时复位

### 5.8 串口调试 (UartTask.c)

- USART2, 波特率 115200
- 输出格式: `ST:LINE_FOLLOW | dis:15.30cm | track:0x06`
- 500ms 周期
- 错误恢复: `HAL_UART_Transmit` 失败时执行 DeInit + reInit

### 5.9 LED 任务 (LedTask.c)

- 每 500ms 翻转 PA12
- 统计上升沿次数，发送到 LEDFlash 队列
- LEDFlash 队列目前无消费者

---

## 6. PID 控制器详情 (pid.c)

**算法**: 位置式 PID，带积分抗饱和

```c
float PID_Compute(PID_HandleTypeDef *pid, float measurement) {
    float error = pid->setpoint - measurement;          // 计算误差
    pid->integral += error;                              // 积分累加
    // 抗饱和：将积分值钳位到输出范围
    if (pid->integral > output_max) integral = output_max;
    if (pid->integral < output_min) integral = output_min;
    float derivative = error - pid->prev_error;          // 微分项
    pid->prev_error = error;
    float output = Kp*error + Ki*integral + Kd*derivative;
    // 输出限幅
    if (output > output_max) output = output_max;
    if (output < output_min) output = output_min;
    return output;
}
```

**PID_Reset**: 将积分和上次误差清零（在状态切换时调用）

---

## 7. 已知问题与 Bug

### 7.1 已修复的问题

| 问题 | 根本原因 | 修复方案 |
|---|---|---|
| RAM 溢出 (107%) | FreeRTOS 堆=16384 超过 20KB 总 RAM | 堆减小到 10240 |
| OLED 不刷新 | I2C HAL_MAX_DELAY 阻塞 | 超时改为 100ms + 总线恢复 |
| 串口只发一次 | 无错误恢复机制 | 失败时 DeInit+reInit |
| 超声波卡在 999 | TIM1 计数器未启动 | 添加 HAL_TIM_Base_Start(&htim1) |
| 四轮同时后退 | 避障循环中距离数据过期 | 失败时重置 distance=0 + 10 秒超时 |
| 循迹抖动 | 继电器式控制（开关控制） | 替换为 PID 差速转向 |
| MotorAction 队列损坏 | sizeof(uint64_t)=8 < sizeof(MotorActionMsg)=12 | 改为 element_size=12 |
| 超声波初始能读后续一直 0 | Distance 队列多消费者耗尽 + Echo 卡在高电平 | 改为全局 g_distance + 预等待 Echo 低电平 |
| TrackTask 串口冲突 | TrackTask 和 UartTask 都使用 UART2 | 移除 TrackTask 中的串口输出 |

### 7.2 残留问题

1. **EchoBinarySem 未使用**: freertos.c 中创建的信号量从未被获取/释放（HCSR04 使用轮询模式）。浪费 80-100 字节 RAM。可以移除。

2. **DriverPWM 队列未使用**: 已创建但从未有生产者/消费者。可以移除以节省 RAM。

3. **DistanceHandle 队列未使用**: 遗留队列，已被 g_distance 全局变量替代。可以移除以节省 RAM。

4. **g_obs_state 非原子访问**: `volatile uint8_t g_obs_state` 被 OledTask 和 UartTask 读取，同时被 CtrlTask 写入。单字节读取在 Cortex-M3 上是原子的，但如果扩展为多字节，需要互斥锁或临界区。

5. **HCSR04 轮询忙等待**: 等待 Echo 引脚边沿的 `while` 循环会阻塞任务而不让出 CPU（每个边沿 30ms 超时）。在此窗口期间低优先级任务被抢占。由于 HCSR04 是最高优先级任务，这是可接受的。

6. **避障转向开环**: 转弯角度基于时间（810ms 转 90°，1590ms 转 180°）。对电池电压、地面摩擦和车轮打滑敏感。编码器可用于闭环转弯但目前未使用。

---

## 8. 内存预算

| 区域 | 大小 | 用途 |
|---|---|---|
| Flash | 64KB | 代码 + 常量数据 |
| RAM 总计 | 20KB | 栈 + 堆 + 全局变量 |
| FreeRTOS 堆 | 10240B (10KB) | 任务栈 + 队列缓冲 |
| 主栈 | 0x400 (1KB) | 中断 + main() |
| 最小堆 | 0x200 (512B) | malloc() |
| OLED 缓冲 | 1024B | OLED_GRAM[8][128]（.bss 段） |
| 任务栈合计 | ~3.5KB | 7 个任务 × 512-640B |
| 队列缓冲合计 | ~1.5KB | 5 个队列 × 16 元素 |
| 剩余 | ~4-5KB | 全局变量、中断栈 |

**RAM 非常紧张。** 不要在不减少其他部分的情况下添加更多任务或更大的缓冲区。

---

## 9. 文件结构

```
clion_car/
├── Core/
│   ├── App/
│   │   └── Tasks/              # 应用逻辑（USER CODE - CubeMX 重新生成代码时保留）
│   │       ├── CtrlTask.c      # 主控制: PID + 避障状态机
│   │       ├── DriverTask.c    # 电机指令执行
│   │       ├── HCSR04.c        # 超声波传感器轮询
│   │       ├── TrackTask.c     # 循迹传感器读取
│   │       ├── OledTask.c      # OLED 显示更新
│   │       ├── UartTask.c      # 串口调试输出
│   │       ├── LedTask.c       # LED 闪烁 + 计数
│   │       ├── pid.c / pid.h   # PID 控制器库
│   │       ├── motor.c / motor.h # 底层电机控制
│   │       └── Encoder.c / Encoder.h # 编码器速度读取
│   ├── Inc/                    # 头文件（CubeMX 管理 + 手动编写）
│   │   ├── main.h              # 引脚定义、外设句柄
│   │   ├── FreeRTOSConfig.h    # FreeRTOS 内核配置
│   │   ├── oled.h              # OLED 驱动 API
│   │   └── font16x16.h         # 字体数据
│   └── Src/                    # CubeMX 生成的源文件
│       ├── freertos.c          # 任务/队列创建（含 USER CODE 区域）
│       ├── main.c              # 系统初始化、外设初始化
│       ├── stm32f1xx_it.c      # 中断处理函数
│       ├── tim.c               # 定时器初始化 (TIM1/2/3)
│       ├── i2c.c               # I2C1 初始化
│       ├── usart.c             # USART2 初始化
│       ├── gpio.c              # GPIO 初始化
│       └── oled.c              # SSD1306 OLED 驱动
├── docs/
│   └── PID.md                  # PID 文档
├── Drivers/                    # HAL + CMSIS 库
├── Middlewares/                # FreeRTOS 内核
├── STM32F103XX_FLASH.ld       # 链接脚本
├── claude_cn.md                # 本文件（中文版）
├── cloud.md                    # 英文版 AI 档案
└── README.md                   # 项目说明
```

### 9.1 CubeMX 重新生成安全性

`Core/Src/` 下的文件（freertos.c 的 USER CODE 区域除外）会被 CubeMX 重新生成。
`Core/App/Tasks/` 下的文件是**安全的** - 不会被 CubeMX 覆盖。
`freertos.c` 有 `/* USER CODE BEGIN/END */` 标记 - 标记之间的代码在重新生成时保留。

**关键**: CubeMX 重新生成后，检查：
- freertos.c 中 USER CODE 区域的任务优先级和队列大小
- 所有定时器/GPIO 配置是否与上述引脚映射一致

---

## 10. 开发指南

### 10.1 添加新任务

1. 在 `Core/App/Tasks/` 创建源文件
2. 在 freertos.c 添加 `extern void StartXxxTask(void *argument);`
3. 在 freertos.c 添加任务属性结构体（在 USER CODE 区域内）
4. 在 MX_FREERTOS_Init() 添加 `osThreadNew()` 调用
5. 考虑 RAM 影响 - 每个任务需要栈 + TCB（约 100B 开销）

### 10.2 修改任务优先级

在 CubeMX 中：Thread → Priority 下拉菜单。值映射到 CMSIS-RTOS：
- osPriorityLow = 12
- osPriorityNormal = 24
- osPriorityAboveNormal = 40
- osPriorityRealtime = 48

或直接在 freertos.c 任务属性中修改（在 USER CODE 区域内修改可保留）。

### 10.3 PID 调参

1. 速度 PID: 先只调 Kp（Ki=0, Kd=0）。从小到大增大，直到响应足够快。加 Ki 消除稳态误差。加 Kd 减少振荡。
2. 转向 PID: Kp 控制灵敏度。太大 = 振荡。Kd 抑制超调。
3. 避障时序: 根据实际电机速度和地面情况调整 CYCLES 常量。

### 10.4 调试流程

1. OLED 显示实时状态（300ms 刷新）
2. 串口输出结构化文本（500ms 周期，波特率 115200）
3. 连接 USB 转串口到 PA2(TX)/PA3(RX)，波特率 115200
4. 监控输出: `ST:LINE_FOLLOW | dis:15.30cm | track:0x06`

---

## 11. 扩展性说明

### 潜在改进（按优先级排列）

1. **闭环避障转向**: 使用编码器实现精确 90°/180° 转弯，替代基于时间的方式
2. **移除 TrackTask 串口输出**: 消除与 UartTask 的冲突
3. **清理未使用的队列**: LEDFlash、DriverPWM、EchoBinarySem → 节省约 400B RAM
4. **超声波中断模式**: 切换到 TIM 输入捕获以获得更可靠的测量
5. **速度斜坡**: 渐进加速替代瞬间 PWM 变化
6. **电池电压补偿**: ADC 读取电池电压，相应调整 PWM

### 添加蓝牙/WiFi

- 可用 UART 引脚: PA2/PA3 已用于调试
- 方案: 将 UART 重映射到其他引脚，或使用 SPI 接口的模块
- 需要额外任务栈（最少 512B）- RAM 预算紧张

### 添加更多传感器

- 有空闲 ADC 引脚可用于模拟传感器
- 每个新任务最少消耗约 600B RAM
- 考虑将 LEDFlash/DriverPWM 队列替换为新队列

---

## 12. 快速参考：关键常量

| 常量 | 值 | 位置 |
|---|---|---|
| TARGET_SPEED | 300.0f | CtrlTask.c |
| OBS_DETECT_DIST | 20.0f cm | CtrlTask.c |
| OBS_TURN_PWM | 700 | CtrlTask.c |
| OBS_FORWARD_PWM | 250 | CtrlTask.c |
| PWM_MAX_VALUE | 1000 | motor.h |
| 控制周期 | 30ms | CtrlTask.c osDelay(30) |
| 超声波周期 | ~100ms | HCSR04.c osDelay(80) |
| 循迹周期 | 80ms | TrackTask.c osDelay(80) |
| OLED 刷新 | 300ms | OledTask.c osDelay(300) |
| 串口输出 | 500ms | UartTask.c osDelay(500) |
| FreeRTOS 堆 | 10240B | FreeRTOSConfig.h |
| TICK_RATE | 1000Hz | FreeRTOSConfig.h |
