# STM32 智能小车控制系统

## 项目概述

本项目是一个基于STM32F103C8T6微控制器的智能小车控制系统，采用FreeRTOS实时操作系统进行任务管理，集成了超声波测距、OLED显示、LED指示和串口通信等功能。

## 硬件配置

### 微控制器
- **芯片**: STM32F103C8T6
- **内核**: ARM Cortex-M3
- **主频**: 72MHz
- **Flash**: 64KB
- **RAM**: 20KB

### 外设配置
| 外设 | 功能 | 引脚配置 |
|------|------|----------|
| TIM1 | 超声波Echo信号捕获 | PA10 (CH3) |
| TIM4 | 系统时基 (1ms中断) | - |
| USART2 | 串口通信 | PA2(_TX), PA3(RX) |
| GPIOA Pin11 | 超声波Trig触发 | 推挽输出 |
| GPIOA Pin12 | LED指示 | 推挽输出 |
| I2C1 | OLED显示屏 | PB6(SCL), PB7(SDA) |

### 超声波传感器 (HC-SR04)
- **Trig引脚**: PA11
- **Echo引脚**: PA10
- **测量范围**: 2cm - 400cm
- **测量精度**: ±3mm
- **工作原理**: 通过测量超声波往返时间来计算距离

## 软件架构

### 操作系统
- **RTOS**: FreeRTOS (CMSIS_OS2接口)
- **调度策略**: 抢占式优先级调度

### 任务列表

| 任务名称 | 优先级 | 栈大小 | 功能描述 |
|----------|--------|--------|----------|
| LedTask | Normal | 128*4 | LED闪烁控制，计数器递增 |
| UartTask | Low | 128*4 | 串口数据收发，命令解析 |
| OledTask | Low | 128*4 | OLED显示距离信息 |
| HCSR04Task | Low | 128*4 | 超声波测距控制 |

### 任务间通信

#### 消息队列
| 队列名称 | 消息类型 | 队列长度 | 功能 |
|----------|----------|----------|------|
| Distance | float | 16 | 传递超声波测量的距离值 |
| LEDFlash | uint32_t | 16 | 传递LED翻转次数 |

## 功能模块

### 1. 超声波测距模块 (HCSR04Task)

**功能**: 控制超声波传感器进行距离测量

**工作流程**:
1. 发送10μs的Trig触发脉冲
2. 配置TIM1通道3为输入捕获模式
3. 捕获Echo信号的上升沿和下降沿
4. 计算高电平持续时间
5. 根据声速计算距离: `distance = (falling_cnt - rising_cnt) * 0.017f` (cm)
6. 将距离值发送到Distance消息队列

**关键参数**:
- TIM1预分频: 71 (1MHz计数频率)
- TIM1周期: 65535
- 测量超时: 150ms
- 测量间隔: 100ms

### 2. 串口通信模块 (UartTask)

**功能**: 通过USART2进行数据收发和命令解析

**通信参数**:
- 波特率: 115200
- 数据位: 8
- 停止位: 1
- 无奇偶校验

**支持的命令**:
```
led freq <value>    设置LED闪烁频率（单位：ms）
```

**示例**:
```
led freq 1000    设置LED每1000ms翻转一次
```

### 3. OLED显示模块 (OledTask)

**功能**: 在OLED屏幕上显示实时距离信息

**显示内容**:
- 当前测量距离（单位：cm）
- 系统状态信息

### 4. LED指示模块 (LedTask)

**功能**: 控制LED指示灯的闪烁

**工作模式**:
- LED每500ms翻转一次（默认）
- 通过串口命令可动态调整闪烁频率
- 每次翻转时将计数器的值发送到LEDFlash消息队列

## 系统初始化流程

1. **HAL初始化**: 配置系统时钟、Systick
2. **外设初始化**: GPIO、I2C、USART、TIM1
3. **FreeRTOS初始化**: 创建消息队列、任务、启动调度器

## 构建说明

### 开发环境
- **IDE**: CLion (或STM32CubeIDE)
- **工具链**: ARM GCC
- **构建系统**: CMake + Ninja

### 构建命令
```bash
# 清理并重新构建
cmake --build build/Debug --target clean
cmake --build build/Debug --target clion_car

# 仅构建
cmake --build build/Debug
```

### 烧录
使用ST-Link或J-Link通过SWD接口烧录`clion_car.elf`文件

## 调试说明

### 串口调试
1. 使用USB转TTL模块连接PA2、PA3
2. 串口调试助手配置: 115200, 8N1
3. 可发送命令控制LED闪烁频率
4. 可接收超声波测距的调试信息

### 调试信息格式
```
shortDebug: rising=1234, falling=5678, distance=12.34 cm
```

## 注意事项

1. **FreeRTOS API使用**: 在FreeRTOS环境下，推荐使用`osDelay()`替代`HAL_Delay()`
2. **中断优先级**: TIM1_CC_IRQn优先级设置为5，避免影响系统实时性
3. **栈空间**: 各任务栈大小为128*4字节，应避免在任务中使用大数组
4. **浮点数处理**: sprintf的%f格式符可能占用较多栈空间，建议使用整数运算替代

## 文件结构

```
Core/
├── App/
│   └── Tasks/
│       ├── HCSR04.c      # 超声波测距任务
│       ├── UartTask.c    # 串口通信任务
│       ├── OledTask.c    # OLED显示任务
│       └── LedTask.c     # LED控制任务
├── Inc/
│   ├── main.h            # 主头文件，引脚定义
│   └── tim.h             # 定时器配置
└── Src/
    ├── main.c            # 主函数，系统初始化
    ├── freertos.c        # FreeRTOS配置，任务创建
    ├── tim.c             # TIM1、TIM4初始化
    ├── usart.c           # USART2初始化
    ├── gpio.c            # GPIO初始化
    └── stm32f1xx_hal_timebase_tim.c  # TIM4时基配置
```

## 常见问题

### 1. 超声波测距超时
- 检查Trig和Echo引脚连接是否正确
- 确保传感器电源稳定（5V）
- 检查目标物体是否在测量范围内

### 2. 串口无输出
- 检查USART2引脚连接
- 确认波特率设置正确（115200）
- 检查USB转TTL模块是否正常工作

### 3. LED不闪烁
- 检查LED连接引脚（PA12）
- 确认LED限流电阻是否合适

## 作者信息
- 创建时间: 2026-04-19
- 开发平台: CLion + STM32CubeMX
