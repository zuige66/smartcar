# STM32F103C8T6 FreeRTOS Smart Car - AI Project Archive

> **Target AI**: This document is optimized for LLM code assistants to understand the full project context without reading every source file.
> **Last Updated**: 2026-05-01

---

## 1. Project Overview

- **MCU**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, **20KB RAM**)
- **RTOS**: FreeRTOS v10.3.1 via CMSIS-RTOS v2 wrapper
- **IDE**: CLion + STM32CubeMX (code generation) + OpenOCD (flash/debug)
- **Function**: Line-following smart car with obstacle avoidance, ultrasonic sensing, OLED display, UART debug
- **Language**: C (C99)

### 1.1 Capabilities

1. 4-channel IR line tracking (PID smooth differential steering)
2. HC-SR04 ultrasonic obstacle detection (**interrupt mode** - TIM1 CH3 input capture)
3. 7-state obstacle avoidance state machine (stop → spin left 90° → check → spin right 180° → check → bypass)
4. SSD1306 128x64 OLED real-time status display
5. UART 115200 debug output
6. TIM3 quadrature encoder for speed feedback
7. Dual-motor L298N driver with PWM speed control

---

## 2. Hardware Pin Map

| Function | Pin | Timer/Peripheral | Notes |
|---|---|---|---|
| Left Motor AIN1 | PA4 | GPIO | Direction control |
| Left Motor AIN2 | PA5 | GPIO | Direction control |
| Right Motor BIN1 | PA6 | GPIO | Direction control |
| Right Motor BIN2 | PA7 | GPIO | Direction control |
| Left Motor PWM | PA0 | TIM2_CH1 | Speed control, 0-1000 |
| Right Motor PWM | PA1 | TIM2_CH2 | Speed control, 0-1000 |
| Encoder Left CH1 | PB4 | TIM3_CH1 | Partial remap |
| Encoder Left CH2 | PB5 | TIM3_CH2 | Partial remap |
| HC-SR04 Echo | PA10 | TIM1_CH3 | **Alternate Function Input** (input capture) |
| HC-SR04 Trig | PA11 | GPIO | Output trigger pulse |
| Track Sensor X1 | PB14 | GPIO | IR sensor input |
| Track Sensor X2 | PB15 | GPIO | IR sensor input |
| Track Sensor X3 | PA8 | GPIO | IR sensor input |
| Track Sensor X4 | PA9 | GPIO | IR sensor input |
| LED | PA12 | GPIO | Status LED |
| I2C SCL | PB6 | I2C1 | OLED clock |
| I2C SDA | PB7 | I2C1 | OLED data |
| UART TX | PA2 | USART2 | Debug output |
| UART RX | PA3 | USART2 | Debug input |

---

## 3. CubeMX Configuration Summary

### 3.1 Clock

- HSE 8MHz external crystal
- PLL: 8MHz × 9 = 72MHz (SYSCLK)
- AHB = 72MHz, APB1 = 36MHz, APB2 = 72MHz

### 3.2 TIM1 (Ultrasonic Timing)

- Prescaler: 71 → 1MHz counter (1 tick = 1µs)
- Period: 65535
- CH3 Input Capture on PA10 (not currently used in polling mode)
- NVIC: TIM1_CC_IRQn priority = 5

### 3.3 TIM2 (Motor PWM)

- Prescaler: 0 → 72MHz
- Period: 999 → PWM frequency = 72kHz
- CH1 (PA0): Left motor PWM
- CH2 (PA1): Right motor PWM
- PWM mode 1, Fast PWM

### 3.4 TIM3 (Encoder)

- Encoder mode on both channels
- Period: 65535
- Partial remap: CH1=PB4, CH2=PB5
- No prescaler, counts every edge

### 3.5 I2C1 (OLED)

- Standard mode, 100kHz
- PB6 (SCL), PB7 (SDA)

### 3.6 USART2 (Debug)

- 115200 baud, 8N1
- PA2 (TX), PA3 (RX)
- No hardware flow control

### 3.7 GPIO Configuration

| Pin | Mode | Pull | Speed |
|---|---|---|---|
| PA4-PA7 (Motor dir) | Output Push-Pull | None | Low |
| PA8, PA9, PB14, PB15 (Track) | Input | None (floating) | - |
| PA10 (Echo) | Input | None (floating) | - |
| PA11 (Trig) | Output Push-Pull | None | Low |
| PA12 (LED) | Output Push-Pull | None | Low |

---

## 4. FreeRTOS Task Architecture

### 4.1 Task List (Priority Descending)

| Task | Priority (CMSIS) | CubeMX Value | Stack | Period | File |
|---|---|---|---|---|---|
| HCSR04Task | osPriorityRealtime (48) | 48 | 512B | ~100ms | HCSR04.c |
| TrackTask | osPriorityAboveNormal (40) | 40 | 512B | 80ms | TrackTask.c |
| DriverTask | osPriorityNormal (24) | 24 | 512B | Event-driven | DriverTask.c |
| CtrlTask | osPriorityNormal (24) | 24 | 512B | 30ms | CtrlTask.c |
| OledTask | osPriorityLow (12) | 12 | 640B | 300ms | OledTask.c |
| UartTask | osPriorityLow1 (13) | 13 | 640B | 500ms | UartTask.c |
| LedTask | osPriorityLow (12) | 12 | 512B | 500ms | LedTask.c |

### 4.2 Data Flow

```
HCSR04Task ──[g_distance 全局变量]──→ CtrlTask ──[MotorActionQueue]──→ DriverTask ──→ Motors
TrackTask  ──[TrackQueue]──────────→ CtrlTask                                   ↑
HCSR04Task ──[g_distance 全局变量]──→ OledTask                            TIM3 Encoder (feedback)
TrackTask  ──[TrackQueue]──────────→ OledTask
HCSR04Task ──[g_distance 全局变量]──→ UartTask
TrackTask  ──[TrackQueue]──────────→ UartTask
```

### 4.3 Shared Data (Global Variables)

| Variable | Type | Writer | Readers | Notes |
|---|---|---|---|---|
| g_distance | volatile float | HCSR04Task | CtrlTask, OledTask, UartTask | 距离值(cm), 0.0=无效 |
| g_obs_state | volatile uint8_t | CtrlTask | OledTask, UartTask | 避障状态机当前状态 |

### 4.4 Queue Definitions

| Queue | Element Size | Depth | Producer | Consumer |
|---|---|---|---|---|
| TrackHandle | sizeof(uint8_t) = 1B | 16 | TrackTask | CtrlTask, OledTask, UartTask |
| MotorActionHandle | 12B (MotorActionMsg) | 16 | CtrlTask | DriverTask |
| DistanceHandle | sizeof(float) = 4B | 16 | (unused, legacy) | (unused, legacy) |
| LEDFlashHandle | sizeof(uint32_t) = 4B | 16 | LedTask | (unused) |
| DriverPWMHandle | sizeof(uint32_t) = 4B | 16 | (unused) | (unused) |

### 4.5 Semaphore

| Semaphore | Type | Status |
|---|---|---|
| EchoBinarySem | Binary | **UNUSED** (created but never taken/given, was for interrupt-based HCSR04) |

### 4.6 Task Scheduling Analysis

| Task | Priority | Period | CPU Time/Exec | Max CPU% | Starvation Risk |
|---|---|---|---|---|---|
| HCSR04Task | 48 (Realtime) | ~100ms | 30-60ms (polling) + 3ms (trig) | ~60% | None (highest) |
| TrackTask | 40 (AboveNormal) | 80ms | <1ms | ~1% | None |
| DriverTask | 24 (Normal) | Event | <1ms | ~1% | None |
| CtrlTask | 24 (Normal) | 30ms | 2-5ms | ~10% | Yes: blocked 30-60ms during HCSR04 polling |
| OledTask | 12 (Low) | 300ms | 10-20ms (I2C) | ~5% | Yes: delayed by higher-priority tasks |
| UartTask | 13 (Low1) | 500ms | 2-5ms | ~1% | Yes: delayed by higher-priority tasks |
| LedTask | 12 (Low) | 500ms | <1ms | ~0.2% | None |

**Key insight**: HCSR04Task polling blocks for 30-60ms every ~100ms. During this window, ALL lower-priority tasks are preempted. This is acceptable because:
- CtrlTask's 30ms deadline may slip by up to 60ms → still ~90ms total, tolerable for motor control
- OledTask/UartTask/LedTask have no real-time requirements
- TrackTask (priority 40) only takes <1ms and can sneak in between HCSR04 trigger and polling

---

## 5. Module Details

### 5.1 HCSR04 Ultrasonic (HCSR04.c)

**Mode**: **Interrupt-based** (TIM1 CH3 input capture)

**Output**: 写入全局变量 `volatile float g_distance`（非队列）

**Measurement principle**:
- TIM1 CH3 (PA10) configured for input capture
- First capture on **rising edge**: record timestamp
- Second capture on **falling edge**: calculate duration
- Duration = falling_timestamp - rising_timestamp
- Distance = duration(us) * 0.017 cm

**Measurement cycle** (~100ms total):
1. Reset capture state machine, set polarity to rising edge
2. Send Trig pulse: LOW 5us → HIGH 15us → LOW
3. Wait for semaphore released by interrupt (timeout 100ms)
4. If successful: calculate distance, range check 2-400cm
5. 5 samples with outlier removal and averaging
6. Write final distance to `g_distance`
7. osDelay(80)

**Interrupt callback**: `HAL_TIM_IC_CaptureCallback()` handles both edges

**Key dependency**: `HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3)` called once at task start

**GPIO critical**: Echo pin (PA10) MUST be configured as `GPIO_MODE_AF_INPUT`, NOT `GPIO_MODE_AF_PP`

### 5.2 Line Tracking (TrackTask.c)

**Sensors**: 4x digital IR (X1=PB14, X2=PB15, X3=PA8, X4=PA9)
- GPIO_PIN_SET = line detected (black line on white surface)
- GPIO_PIN_RESET = no line

**Status byte**: `(X4<<3) | (X3<<2) | (X2<<1) | (X1<<0)`

**Period**: 80ms (was 50ms, reduced to match sensor response time)

**Output**: 仅写入 TrackHandle 队列（UART 输出已移除，由 UartTask 统一处理）

### 5.3 Control Task (CtrlTask.c)

**Core decision loop** at 30ms period:

1. Read `g_distance` global + Track queue (non-blocking)
2. Read encoder speed, reset counter
3. Calculate track error via `CalculateTrackError()`
4. Run obstacle avoidance state machine
5. Compute PID: speed PID → base PWM, turn PID → differential correction
6. Send MotorActionMsg to DriverTask

**MotorActionMsg struct** (12 bytes):
```c
typedef struct {
    MotorCmdTypeDef cmd;     // enum: STOP=0, FORWARD=1, TURN_LEFT=2, TURN_RIGHT=3, SPIN_LEFT=4, SPIN_RIGHT=5
    uint16_t pwm;            // Obstacle avoidance PWM (0-1000)
    uint16_t pwm_left;       // Left wheel PWM for line follow (0-1000)
    uint16_t pwm_right;      // Right wheel PWM for line follow (0-1000)
} MotorActionMsg;
```

**PID parameters**:
- Speed PID: Kp=2.0, Ki=0.1, Kd=0.5, target=300 encoder pulses, output=[0,600]
- Turn PID: Kp=80.0, Ki=0.0, Kd=20.0, target=0, output=[-250,250]

**Track error calculation** (4-bit status → error -3 to +3):

| Hex | Binary | Error | Meaning |
|---|---|---|---|
| 0x06 | 0110 | 0 | Centered |
| 0x09 | 1001 | 0 | Centered |
| 0x05 | 0101 | 0 | Centered |
| 0x0A | 1010 | 0 | Centered |
| 0x02 | 0010 | -1 | Slightly left |
| 0x01 | 0001 | -2 | Left |
| 0x03 | 0011 | -2 | Left |
| 0x0D | 1101 | -3 | Far left |
| 0x0E | 1110 | -3 | Far left |
| 0x04 | 0100 | +1 | Slightly right |
| 0x08 | 1000 | +2 | Right |
| 0x0C | 1100 | +2 | Right |
| 0x0B | 1011 | +3 | Far right |
| 0x00 | 0000 | 0 | Lost (all off) |
| 0x0F | 1111 | 0 | Cross (all on) |

**PID smooth differential steering** (line follow mode):
```
base_pwm = speed_PID_output (min 50)
turn_output = turn_PID_output (-250 to +250)
left_pwm  = base_pwm + turn_output  (clamped 50-600)
right_pwm = base_pwm - turn_output  (clamped 50-600)
```

### 5.4 Obstacle Avoidance State Machine

```
LINE_FOLLOW ──(distance≤20cm)──→ OBS_STOP (7 cycles = 210ms)
     ↑                                │
     │                                ↓
     │                          OBS_TURN_LEFT (27 cycles = 810ms, spin left)
     │                                │
     │                                ↓
     │                          OBS_CHECK_LEFT (3 cycles = 90ms, read distance)
     │                             ╱           ╲
     │                    distance>20cm         distance≤20cm
     │                      ↓                       ↓
     │              OBS_FORWARD_BYPASS        OBS_TURN_RIGHT (53 cycles = 1590ms, spin right)
     │              (33 cycles = 990ms)              │
     │                      │                       ↓
     │                      │               OBS_CHECK_RIGHT (3 cycles)
     │                      │                  ╱           ╲
     │                 complete           distance>20cm   distance≤20cm
     │                      ↓               ↓               ↓
     └──────────────────────┘          OBS_FORWARD      LINE_FOLLOW (dead end)
                                             │
                                             ↓
                                       OBS_FORWARD_BYPASS
```

**Obstacle avoidance parameters**:

| Parameter | Value | Description |
|---|---|---|
| OBS_DETECT_DIST | 20.0f cm | Trigger distance |
| OBS_TURN_PWM | 700 | Spin turn power |
| OBS_FORWARD_PWM | 250 | Bypass forward power |
| OBS_STOP_CYCLES | 7 (210ms) | Stabilization |
| OBS_TURN_90_CYCLES | 27 (810ms) | 90° spin time |
| OBS_TURN_180_CYCLES | 53 (1590ms) | 180° spin time |
| OBS_BYPASS_CYCLES | 33 (990ms) | Bypass forward time |
| OBS_CHECK_WAIT | 3 (90ms) | Wait for fresh distance data |
| OBS_TOTAL_TIMEOUT | 333 (10s) | Abort obstacle avoidance |

**Timeout protection**: If obstacle avoidance exceeds 10s, force return to LINE_FOLLOW with PID reset.

### 5.5 Motor Driver (motor.c)

**L298N-style control**:
- Forward: AIN1=HIGH, AIN2=LOW (and BIN1=HIGH, BIN2=LOW)
- Backward: AIN1=LOW, AIN2=HIGH (and BIN1=LOW, BIN2=HIGH)
- Stop: AIN1=HIGH, AIN2=HIGH (both HIGH = brake)
- PWM: TIM2 CH1 (left), TIM2 CH2 (right), 0-1000 range

**Motor_SetSpeed**: `pwm_val = (speed * PWM_MAX_VALUE) / 1000` → compare register

**DriverTask command execution**:

| Command | Left Direction | Right Direction | Left PWM | Right PWM |
|---|---|---|---|---|
| STOP | - | - | 0 | 0 |
| FORWARD | Forward | Forward | pwm_left | pwm_right |
| TURN_LEFT | Forward | Forward | pwm/3 | pwm |
| TURN_RIGHT | Forward | Forward | pwm | pwm/3 |
| SPIN_LEFT | **Backward** | Forward | pwm | pwm |
| SPIN_RIGHT | Forward | **Backward** | pwm | pwm |

### 5.6 Encoder (Encoder.c)

- TIM3 in encoder mode, counts both edges of both channels
- PB4/PB5 (partial remap)
- `Encoder_GetSpeed()`: returns int16_t counter value (signed, negative = reverse)
- `Encoder_Reset()`: sets counter to 0
- Called every 30ms by CtrlTask

### 5.7 OLED Display (OledTask.c)

- SSD1306 128x64 via I2C (address 0x78)
- 1024-byte frame buffer: `OLED_GRAM[8][128]`
- font16x16 (16px height, 4 lines = 64px full screen)
- Display content: obstacle state, distance, track hex, X1X2X3X4 bits
- Refresh rate: 300ms
- I2C timeout: 100ms (was MAX_DELAY, caused blocking)
- Error recovery: I2C bus reset on failure

### 5.8 UART Debug (UartTask.c)

- USART2 at 115200 baud
- Output format: `ST:LINE_FOLLOW | dis:15.30cm | track:0x06`
- 500ms cycle
- Error recovery: DeInit + reInit on HAL_UART_Transmit failure

### 5.9 LED Task (LedTask.c)

- Toggle PA12 every 500ms
- Counts rising edges, sends count to LEDFlash queue
- LEDFlash queue is currently consumed by nothing

---

## 6. PID Controller Details (pid.c)

**Algorithm**: Positional PID with integral anti-windup

```c
float PID_Compute(PID_HandleTypeDef *pid, float measurement) {
    float error = pid->setpoint - measurement;
    pid->integral += error;
    // Anti-windup: clamp integral to output limits
    if (pid->integral > output_max) integral = output_max;
    if (pid->integral < output_min) integral = output_min;
    float derivative = error - pid->prev_error;
    pid->prev_error = error;
    float output = Kp*error + Ki*integral + Kd*derivative;
    // Output clamp
    if (output > output_max) output = output_max;
    if (output < output_min) output = output_min;
    return output;
}
```

**PID_Reset**: zeros integral and prev_error (called on state transitions)

---

## 7. Known Issues and Bugs

### 7.1 FIXED Issues

| Issue | Root Cause | Fix |
|---|---|---|
| RAM overflow (107%) | FreeRTOS heap=16384 exceeded 20KB total RAM | Reduced heap to 10240 |
| OLED not refreshing | I2C HAL_MAX_DELAY blocking | Reduced to 100ms timeout + bus recovery |
| UART one-shot | No error recovery | Added DeInit+reInit on failure |
| Ultrasonic stuck 999 | TIM1 counter not started | Added HAL_TIM_Base_Start(&htim1) |
| All wheels backward | Stale distance in obstacle loop | Reset distance=0 on failure + 10s timeout |
| Line follow jerky | Bang-bang control | Replaced with PID differential steering |
| MotorAction queue corruption | sizeof(uint64_t)=8 < sizeof(MotorActionMsg)=12 | Changed to element_size=12 |
| Ultrasonic shows 0 after working | Distance queue multi-consumer drain + Echo stuck HIGH | Changed to global g_distance + pre-wait for Echo LOW |
| TrackTask UART conflict | TrackTask and UartTask both used UART2 | Removed UART from TrackTask |

### 7.2 REMAINING Issues

1. **EchoBinarySem unused**: Semaphore created in freertos.c but never used (HCSR04 is polling mode). Wastes 80-100 bytes RAM. Can be removed.

2. **DriverPWM queue unused**: Created but never produced/consumed. Can be removed to save RAM.

3. **DistanceHandle queue unused**: Legacy queue, replaced by g_distance global. Can be removed to save RAM.

4. **g_obs_state non-atomic**: `volatile uint8_t g_obs_state` is read by OledTask and UartTask while written by CtrlTask. Single-byte reads are atomic on Cortex-M3, but if this expands to multi-byte, it needs a mutex or critical section.

5. **HCSR04 polling busy-wait**: The `while` loops waiting for Echo pin edges block the task without yielding (30ms timeout per edge). During this window, lower-priority tasks are starved. Acceptable since HCSR04 is the highest priority task.

6. **Obstacle avoidance open-loop turning**: Turn angles are time-based (810ms for 90°, 1590ms for 180°). These are sensitive to battery voltage, surface friction, and wheel slip. The encoder could be used for closed-loop turning but isn't.

---

## 8. Memory Budget

| Region | Size | Usage |
|---|---|---|
| Flash | 64KB | Code + const data |
| RAM total | 20KB | Stack + heap + globals |
| FreeRTOS heap | 10240B (10KB) | Task stacks + queue buffers |
| Main stack | 0x400 (1KB) | ISR + main() |
| Min heap | 0x200 (512B) | malloc() |
| OLED buffer | 1024B | OLED_GRAM[8][128] in .bss |
| Task stacks | ~3.5KB total | 7 tasks × 512-640B |
| Queue buffers | ~1.5KB | 5 queues × 16 elements |
| Remaining | ~4-5KB | Global variables, ISR stack |

**RAM is very tight.** Do not add more tasks or larger buffers without reducing elsewhere.

---

## 9. File Structure

```
clion_car/
├── Core/
│   ├── App/
│   │   └── Tasks/              # Application logic (USER CODE - survives CubeMX regen)
│   │       ├── CtrlTask.c      # Main control: PID + obstacle avoidance
│   │       ├── DriverTask.c    # Motor command execution
│   │       ├── HCSR04.c        # Ultrasonic sensor polling
│   │       ├── TrackTask.c     # Line tracking sensor reading
│   │       ├── OledTask.c      # OLED display update
│   │       ├── UartTask.c      # UART debug output
│   │       ├── LedTask.c       # LED blink + count
│   │       ├── pid.c / pid.h   # PID controller library
│   │       ├── motor.c / motor.h # Low-level motor control
│   │       └── Encoder.c / Encoder.h # Encoder speed reading
│   ├── Inc/                    # Headers (CubeMX-managed + manual)
│   │   ├── main.h              # Pin definitions, peripheral handles
│   │   ├── FreeRTOSConfig.h    # FreeRTOS kernel configuration
│   │   ├── oled.h              # OLED driver API
│   │   └── font16x16.h         # Font data
│   └── Src/                    # CubeMX-generated sources
│       ├── freertos.c          # Task/queue creation (USER CODE sections)
│       ├── main.c              # System init, peripheral init
│       ├── stm32f1xx_it.c      # Interrupt handlers
│       ├── tim.c               # Timer init (TIM1/2/3)
│       ├── i2c.c               # I2C1 init
│       ├── usart.c             # USART2 init
│       ├── gpio.c              # GPIO init
│       └── oled.c              # SSD1306 OLED driver
├── docs/
│   └── PID.md                  # PID documentation
├── Drivers/                    # HAL + CMSIS libraries
├── Middlewares/                # FreeRTOS kernel
├── STM32F103XX_FLASH.ld       # Linker script
├── cloud.md                    # THIS FILE
└── README.md                   # Project readme
```

### 9.1 CubeMX Regeneration Safety

Files in `Core/Src/` (except freertos.c USER CODE sections) are regenerated by CubeMX.
Files in `Core/App/Tasks/` are **safe** - never overwritten by CubeMX.
`freertos.c` has `/* USER CODE BEGIN/END */` markers - code between them survives regeneration.

**Critical**: After CubeMX regeneration, check:
- freertos.c: Task priorities and queue sizes in USER CODE sections
- All timer/GPIO configurations match pin map above

---

## 10. Development Guidelines

### 10.1 Adding a New Task

1. Create source file in `Core/App/Tasks/`
2. Add `extern void StartXxxTask(void *argument);` in freertos.c
3. Add task attributes struct in freertos.c (inside USER CODE sections)
4. Add `osThreadNew()` call in MX_FREERTOS_Init()
5. Consider RAM impact - each task needs stack + TCB (~100B overhead)

### 10.2 Modifying Task Priorities

In CubeMX: Thread → Priority dropdown. Values map to CMSIS-RTOS:
- osPriorityLow = 12
- osPriorityNormal = 24
- osPriorityAboveNormal = 40
- osPriorityRealtime = 48

Or modify directly in freertos.c task attributes (survives regeneration if in USER CODE).

### 10.3 PID Tuning

1. Speed PID: Start with Kp only (Ki=0, Kd=0). Increase until response is fast. Add Ki to eliminate steady-state error. Add Kd to reduce oscillation.
2. Turn PID: Kp controls responsiveness. Too high = oscillation. Kd suppresses overshoot.
3. Obstacle avoidance timing: Adjust CYCLES constants based on actual motor speed and surface.

### 10.4 Debug Workflow

1. OLED shows real-time state (300ms refresh)
2. UART outputs structured text (500ms cycle, 115200 baud)
3. Connect USB-UART to PA2(TX)/PA3(RX) at 115200
4. Monitor: `ST:LINE_FOLLOW | dis:15.30cm | track:0x06`

---

## 11. Extensibility Notes

### Potential Improvements (by priority)

1. **Closed-loop obstacle turning**: Use encoder for precise 90°/180° turns instead of time-based
2. **Remove TrackTask UART**: Eliminate conflict with UartTask
3. **Clean up unused queues**: LEDFlash, DriverPWM, EchoBinarySem → save ~400B RAM
4. **Ultrasonic interrupt mode**: Switch to TIM input capture for more reliable measurement
5. **Speed ramp-up**: Gradual acceleration instead of instant PWM changes
6. **Battery voltage compensation**: ADC read battery, adjust PWM accordingly

### Adding Bluetooth/WiFi

- Free UART pins: PA2/PA3 already used for debug
- Option: Remap UART to other pins, or use SPI-based module
- Need additional task stack (512B min) - RAM budget is tight

### Adding More Sensors

- Free ADC pins available for analog sensors
- Each new task costs ~600B RAM minimum
- Consider replacing LEDFlash/DriverPWM queues with new ones

---

## 12. Quick Reference: Key Constants

| Constant | Value | Location |
|---|---|---|
| TARGET_SPEED | 300.0f | CtrlTask.c |
| OBS_DETECT_DIST | 20.0f cm | CtrlTask.c |
| OBS_TURN_PWM | 700 | CtrlTask.c |
| OBS_FORWARD_PWM | 250 | CtrlTask.c |
| PWM_MAX_VALUE | 1000 | motor.h |
| Control period | 30ms | CtrlTask.c osDelay(30) |
| Ultrasonic cycle | ~100ms | HCSR04.c osDelay(80) |
| Track cycle | 80ms | TrackTask.c osDelay(80) |
| OLED refresh | 300ms | OledTask.c osDelay(300) |
| UART output | 500ms | UartTask.c osDelay(500) |
| FreeRTOS heap | 10240B | FreeRTOSConfig.h |
| TICK_RATE | 1000Hz | FreeRTOSConfig.h |
