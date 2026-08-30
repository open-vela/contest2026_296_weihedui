# 电机控制使用指南

## 📅 更新时间: 2026-08-30

---

## 🎯 概述

本模块负责门锁电机的控制，包括：
- 开门电机控制
- 关门电机控制
- 锁门电机控制
- 蜂鸣器报警

---

## 🔌 硬件连接

| 功能 | GPIO引脚 | 说明 |
|------|----------|------|
| 开门电机 | GPIO 26 | 高电平触发 |
| 关门电机 | GPIO 27 | 高电平触发 |
| 锁门电机 | GPIO 28 | 高电平触发 |
| 蜂鸣器 | GPIO 29 | 高电平触发 |

---

## 📚 API说明

### door_control_init()

初始化电机控制模块。

```c
int door_control_init(const motor_config_t *config);
```

**参数**:
- `config`: 电机配置参数

**返回值**:
- `0`: 成功
- `-1`: 失败

**示例**:
```c
motor_config_t config = {
    .open_gpio_pin = 26,
    .close_gpio_pin = 27,
    .lock_gpio_pin = 28,
    .buzzer_gpio_pin = 29,
    .open_timeout_ms = 10000,   /* 10秒 */
    .close_timeout_ms = 15000,  /* 15秒 */
    .lock_timeout_ms = 5000     /* 5秒 */
};

door_control_init(&config);
```

---

### motor_control()

控制电机运行。

```c
int motor_control(motor_cmd_t cmd);
```

**参数**:
- `cmd`: 电机命令

**电机命令**:
| 命令 | 说明 |
|------|------|
| `MOTOR_CMD_OPEN` | 开门 |
| `MOTOR_CMD_CLOSE` | 关门 |
| `MOTOR_CMD_LOCK` | 锁门 |
| `MOTOR_CMD_STOP` | 停止 |
| `MOTOR_CMD_EMERGENCY_STOP` | 紧急停止 |

**返回值**:
- `0`: 成功
- `-1`: 失败

**示例**:
```c
/* 开门 */
motor_control(MOTOR_CMD_OPEN);

/* 等待开门完成 */
sleep(5);

/* 停止电机 */
motor_control(MOTOR_CMD_STOP);
```

---

### motor_get_state()

获取当前电机状态。

```c
motor_state_t motor_get_state(void);
```

**返回值**:
| 状态 | 说明 |
|------|------|
| `MOTOR_STATE_IDLE` | 空闲 |
| `MOTOR_STATE_OPENING` | 开门中 |
| `MOTOR_STATE_CLOSING` | 关门中 |
| `MOTOR_STATE_LOCKING` | 锁门中 |
| `MOTOR_STATE_ERROR` | 错误 |

**示例**:
```c
motor_state_t state = motor_get_state();
printf("Current state: %s\n", motor_state_to_string(state));
```

---

### buzzer_alarm()

启动蜂鸣器报警。

```c
int buzzer_alarm(uint32_t duration_ms);
```

**参数**:
- `duration_ms`: 报警持续时间（毫秒）

**返回值**:
- `0`: 成功
- `-1`: 失败

**示例**:
```c
/* 报警500毫秒 */
buzzer_alarm(500);
```

---

### buzzer_stop()

停止蜂鸣器报警。

```c
int buzzer_stop(void);
```

**返回值**:
- `0`: 成功
- `-1`: 失败

---

## 🔄 状态机

```
                    ┌─────────────┐
                    │    IDLE     │
                    └──────┬──────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          ▼                ▼                ▼
   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
   │   OPENING   │  │   CLOSING   │  │   LOCKING   │
   └──────┬──────┘  └──────┬──────┘  └──────┬──────┘
          │                │                │
          └────────────────┼────────────────┘
                           │
                    ┌──────┴──────┐
                    │    IDLE     │
                    └─────────────┘

   任何状态 + EMERGENCY_STOP → IDLE
   任何状态 + 超时 → ERROR → IDLE
```

---

## ⏱️ 超时保护

| 操作 | 超时时间 | 说明 |
|------|----------|------|
| 开门 | 10秒 | 超时后自动停止并报警 |
| 关门 | 15秒 | 超时后自动停止并报警 |
| 锁门 | 5秒 | 超时后自动停止并报警 |

**超时处理流程**:
1. 检测到超时
2. 立即停止所有电机
3. 设置错误状态
4. 触发蜂鸣器报警

---

## ⚠️ 注意事项

1. **安全第一**: 所有电机操作必须有超时保护
2. **互斥访问**: 多线程环境下需要使用互斥锁保护
3. **错误处理**: 异常情况要立即停止电机
4. **状态一致**: 状态机转换要正确

---

## 🔧 常见问题

### Q: 电机不转怎么办？
A: 检查以下几点：
1. GPIO配置是否正确
2. GPIO电平是否正常
3. 电机驱动电路是否正常
4. 电源是否正常

### Q: 电机抖动怎么办？
A: 可能的原因：
1. PWM参数不合适
2. 电源不稳定
3. 机械结构卡滞

### Q: 超时报警怎么办？
A: 检查以下几点：
1. 机械结构是否正常
2. 超时时间是否合适
3. 电机是否过载

---

## 📝 使用示例

### 完整示例

```c
#include "door_control.h"

int main(void)
{
    /* 配置电机参数 */
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    /* 初始化 */
    door_control_init(&config);

    /* 开门 */
    printf("Opening door...\n");
    motor_control(MOTOR_CMD_OPEN);
    sleep(5);

    /* 停止 */
    motor_control(MOTOR_CMD_STOP);
    sleep(1);

    /* 关门 */
    printf("Closing door...\n");
    motor_control(MOTOR_CMD_CLOSE);
    sleep(5);

    /* 锁门 */
    printf("Locking door...\n");
    motor_control(MOTOR_CMD_LOCK);
    sleep(2);

    /* 反初始化 */
    door_control_deinit();

    return 0;
}
```

---

*最后更新: 2026-08-30*
