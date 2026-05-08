# STM32F103 智能小车 CubeMX 配置说明

## 一、项目概述

本项目是一个基于 STM32F103C8T6 的循迹避障智能小车，使用 FreeRTOS 操作系统。

### 硬件资源分配

| 外设 | 功能 | 引脚 | 配置要点 |
|------|------|------|----------|
| TIM1 | 超声波输入捕获 | PA10 (CH3) | 输入捕获模式，上升/下降沿 |
| TIM2 | 电机PWM输出 | PA0 (CH1), PA1 (CH2) | PWM模式，10kHz |
| TIM3 | 编码器 | PB4 (CH1), PB5 (CH2) | 编码器模式，部分重映射 |
| USART2 | 串口调试 | PA2 (TX), PA3 (RX) | 115200bps, 8N1 |
| I2C1 | OLED显示 | PB6 (SCL), PB7 (SDA) | 标准模式，100kHz |
| GPIO | 电机控制 | PA4, PA5, PA6, PA7 | 推挽输出 |
| GPIO | 循迹传感器 | PB0, PB1, PB10, PB11 | 上拉输入 |
| GPIO | 超声波Trig | PA11 | 推挽输出 |
| GPIO | 超声波Echo | PA10 | **复用输入** (TIM1_CH3) |

---

## 二、CubeMX 配置步骤

### 2.1 基础配置

1. **芯片选择**: STM32F103C8T6
2. **时钟配置**: 72MHz (HSE 8MHz)
3. **调试接口**: SWD (PA13, PA14)

### 2.2 TIM1 - 超声波输入捕获

**关键配置项：**

- **模式**: Input Capture
- **Channel 3**: 使能
- **Polarity**: Rising Edge (上升沿触发)
- **Prescaler**: 71 (72MHz / 72 = 1MHz)
- **Counter Period**: 65535
- **IC Filter**: 0 (无滤波)
- **NVIC**: 使能 TIM1_CC_IRQn，优先级 5

**GPIO 配置 (PA10 - TIM1_CH3)**:
- Mode: **Alternate Function Input** (复用输入模式！)
- Pull: No Pull
- Speed: High

> ⚠️ **重要**: Echo 引脚必须配置为 `GPIO_MODE_AF_INPUT`，不能是 AF_PP (输出模式)！

### 2.3 TIM2 - 电机 PWM 输出

**关键配置项：**

- **模式**: PWM Generation CH1 & CH2
- **Prescaler**: 36 (36MHz / 36 = 1MHz)
- **Counter Period**: 999 (1kHz PWM)
- **CH1/CH2 Mode**: PWM Mode 1
- **Pulse**: 0 (初始占空比 0%)

**GPIO 配置 (PA0, PA1)**:
- Mode: Alternate Function Push Pull
- Pull: No Pull
- Speed: High

### 2.4 TIM3 - 编码器模式

**关键配置项：**

- **模式**: Encoder Mode
- **Encoder Mode**: TI1 and TI2
- **Counter Period**: 65535
- **CH1/CH2 Polarity**: Rising

**GPIO 配置 (PB4, PB5)**:
- **部分重映射**: 必须开启 TIM3 部分重映射
- Mode: Input
- Pull: Up (上拉)

**AFIO 配置**:
- Enable AFIO Clock
- Enable TIM3 Partial Remap

### 2.5 USART2 - 串口调试

**关键配置项：**

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Mode**: TX and RX

**GPIO 配置 (PA2, PA3)**:
- PA2 (TX): Alternate Function Push Pull
- PA3 (RX): Input
- Pull: Up (RX)

### 2.5 I2C1 - OLED 显示

**关键配置项：**

- **模式**: I2C
- **Speed Mode**: Fast Mode (400kHz)
- **Clock Number**: 36 (400kHz 所需时钟数)

**GPIO 配置 (PB6, PB7)**:
- Mode: Alternate Function Open Drain
- Pull: Up
- Speed: High

### 2.7 GPIO 配置

#### 电机控制引脚 (PA4-PA7)
| 引脚 | 功能 | 配置 |
|------|------|------|
| PA4 | AIN1 (左电机方向) | Output Push Pull |
| PA5 | AIN2 (左电机方向) | Output Push Pull |
| PA6 | BIN1 (右电机方向) | Output Push Pull |
| PA7 | BIN2 (右电机方向) | Output Push Pull |

#### 循迹传感器引脚 (PB0, PB1, PB10, PB11)
| 引脚 | 功能 | 配置 |
|------|------|------|
| PB0 | X1 (最左) | Input Pull Up |
| PB1 | X2 (左) | Input Pull Up |
| PB10 | X3 (右) | Input Pull Up |
| PB11 | X4 (最右) | Input Pull Up |

#### 超声波 Trig 引脚 (PA11)
- Mode: Output Push Pull
- Pull: No Pull
- Speed: High

### 2.8 FreeRTOS 配置

**任务配置：**

| 任务名称 | 优先级 | 栈大小 | 说明 |
|----------|--------|--------|------|
| CtrlTask | osPriorityNormal | 128 | 主控制任务 |
| DriverTask | osPriorityAboveNormal | 128 | 电机驱动 |
| HCSR04Task | osPriorityRealtime | 256 | 超声波测距 |
| TrackTask | osPriorityLow | 128 | 循迹传感器 |
| OledTask | osPriorityLow | 160 | OLED显示 |
| UartTask | osPriorityLow | 160 | 串口调试 |
| LedTask | osPriorityLow | 128 | LED闪烁 |

**队列配置：**

| 队列名称 | 消息数 | 消息大小 | 说明 |
|----------|--------|----------|------|
| Track | 16 | uint8_t | 循迹数据 |
| Distance | 16 | float | 距离数据 |
| MotorAction | 16 | 64-bit | 电机指令 |

**信号量配置：**

| 信号量名称 | 类型 | 说明 |
|------------|------|------|
| EchoBinarySem | Binary | 超声波捕获完成信号 |

---

## 三、代码生成设置

### 3.1 Project Manager

- **Toolchain/IDE**: STM32CubeIDE (or CLion with CMake)
- **Generate Under Root**: 勾选
- **Generate Peripheral Initialization as**: 勾选 "Call HAL_Init() function in main()"

### 3.2 Clock Tree

```
HSE: 8MHz
PLLMUL: x9 (72MHz)
AHB Prescaler: 1
APB1 Prescaler: 2 (36MHz)
APB2 Prescaler: 1 (72MHz)

TIM1 clock: 72MHz
TIM2 clock: 36MHz
TIM3 clock: 36MHz
```

---

## 四、常见配置错误

### 4.1 超声波不工作

**问题**: 输入捕获中断不触发，一直超时

**可能原因**:

1. **Echo 引脚配置错误**
   - ❌ 错误: `GPIO_MODE_AF_PP` (复用推挽输出)
   - ✅ 正确: `GPIO_MODE_AF_INPUT` (复用输入)

2. **捕获极性错误**
   - 检查 `CCER` 寄存器: `CC3P` 位应为 0 (上升沿)
   - 如果 `CCER=0x0300`，说明极性被设为下降沿

3. **TIM1 未正确启动**
   - 必须调用 `HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3)`

### 4.2 编码器计数异常

**问题**: 编码器读数为0或异常

**可能原因**:

1. **重映射未开启**
   - 必须在 AFIO 配置中开启 TIM3 部分重映射
   - 否则 PB4/PB5 不是 TIM3_CH1/CH2

2. **上拉电阻未配置**
   - 编码器引脚需要上拉输入配置

### 4.3 PWM 输出异常

**问题**: 电机不转或转速异常

**可能原因**:

1. **GPIO 模式错误**
   - PWM 输出引脚必须配置为 `Alternate Function Push Pull`

2. **定时器未启动**
   - 必须调用 `HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1)`

---

## 五、配置验证

### 5.1 启动时检查（可添加到任务初始化）

```c
// 检查 TIM1 输入捕获配置
HAL_StatusTypeDef ret = HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3);
printf("IC_START=%d | CR1=%04X | CCER=%04X | DIER=%04X\r\n",
       ret, TIM1->CR1, TIM1->CCER, TIM1->DIER);

// 正常输出应该是：
// IC_START=0 | CR1=0001 | CCER=0100 | DIER=0008
```

### 5.2 寄存器位说明

| 寄存器 | 位 | 含义 | 期望值 |
|--------|----|------|--------|
| TIM1->CR1 | BIT0 (CEN) | 定时器使能 | 1 |
| TIM1->CCER | BIT8 (CC3E) | 通道3使能 | 1 |
| TIM1->CCER | BIT9 (CC3P) | 捕获极性 | 0 (上升沿) |
| TIM1->DIER | BIT3 (CC3IE) | 通道3中断使能 | 1 |

---

## 六、配置文件清单

配置完成后，CubeMX 会生成以下关键文件：

```
Core/Src/
├── gpio.c          # GPIO 初始化
├── tim.c           # 定时器初始化
├── usart.c         # 串口初始化
├── i2c.c           # I2C 初始化
├── freertos.c      # FreeRTOS 任务/队列/信号量
└── main.c          # 主函数
```

---

## 七、代码修改注意事项

> ⚠️ **重要**: 如果修改了代码中的硬件配置（如 GPIO 模式、定时器配置等），必须同步更新 CubeMX 配置！

### 7.1 修改流程

1. 在 CubeMX 中修改配置
2. 重新生成代码
3. 检查生成的 `gpio.c`, `tim.c` 等文件
4. 如果有自定义代码，手动合并到新生成的文件中

### 7.2 不建议手动修改的文件

以下文件由 CubeMX 自动生成，手动修改后下次生成会被覆盖：

- `Core/Src/gpio.c`
- `Core/Src/tim.c`
- `Core/Src/usart.c`
- `Core/Src/i2c.c`
- `Core/Src/freertos.c`

### 7.3 建议手动修改的位置

在以下位置添加自定义代码，不会被覆盖：

```c
/* USER CODE BEGIN xxx */
// 自定义代码
/* USER CODE END xxx */
```