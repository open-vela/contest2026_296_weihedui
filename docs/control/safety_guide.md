# 防夹安全模块使用指南

## 📅 更新时间: 2026-08-30

---

## 🎯 概述

本模块负责门锁的防夹保护功能，包括：
- 雷达数据实时监测
- 防夹判断逻辑
- 紧急响应机制
- 状态恢复

---

## 🔧 工作原理

### 检测流程

```
┌─────────────────────────────────────────────────────────────┐
│                    防夹检测流程                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐                                           │
│  │ 开始检测    │                                           │
│  └──────┬──────┘                                           │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────┐    否                                     │
│  │ 是否关门中？ │───────────┐                               │
│  └──────┬──────┘           │                               │
│         │ 是               │                               │
│         ▼                   │                               │
│  ┌─────────────┐           │                               │
│  │ 读取雷达数据 │           │                               │
│  └──────┬──────┘           │                               │
│         │                   │                               │
│         ▼                   │                               │
│  ┌─────────────┐    否     │                               │
│  │ 检测到目标？ │───────┐   │                               │
│  └──────┬──────┘       │   │                               │
│         │ 是           │   │                               │
│         ▼               │   │                               │
│  ┌─────────────┐       │   │                               │
│  │ 计数器+1    │       │   │                               │
│  └──────┬──────┘       │   │                               │
│         │               │   │                               │
│         ▼               │   │                               │
│  ┌─────────────┐  否   │   │                               │
│  │ 计数>=3？   │───┐   │   │                               │
│  └──────┬──────┘   │   │   │                               │
│         │ 是       │   │   │                               │
│         ▼           │   │   │                               │
│  ┌─────────────┐   │   │   │                               │
│  │ 触发防夹！  │   │   │   │                               │
│  │ - 停止电机  │   │   │   │                               │
│  │ - 反向开门  │   │   │   │                               │
│  │ - 蜂鸣器    │   │   │   │                               │
│  └──────┬──────┘   │   │   │                               │
│         │           │   │   │                               │
│         ▼           │   │   │                               │
│  ┌─────────────┐   │   │   │                               │
│  │ 等待目标离开 │   │   │   │                               │
│  └──────┬──────┘   │   │   │                               │
│         │           │   │   │                               │
│         └───────────┼───┘   │                               │
│                     │       │                               │
│                     └───────┘                               │
│                             │                               │
│                             ▼                               │
│                      ┌─────────────┐                       │
│                      │ 重置计数器  │                       │
│                      └─────────────┘                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📚 API说明

### safety_init()

初始化安全模块。

```c
int safety_init(const safety_config_t *config);
```

**参数**:
- `config`: 安全配置参数（可选，为NULL使用默认配置）

**默认配置**:
- 防夹阈值: 3次
- 检测间隔: 20ms
- 检测范围: 100cm

**返回值**:
- `0`: 成功
- `-1`: 失败

**示例**:
```c
/* 使用默认配置 */
safety_init(NULL);

/* 或使用自定义配置 */
safety_config_t config = {
    .anti_pinch_threshold = 5,
    .check_interval_ms = 10,
    .detection_range_cm = 150
};
safety_init(&config);
```

---

### safety_start()

启动防夹检测。

```c
int safety_start(void);
```

**返回值**:
- `0`: 成功
- `-1`: 失败

**示例**:
```c
safety_start();
```

---

### safety_stop()

停止防夹检测。

```c
int safety_stop(void);
```

**返回值**:
- `0`: 成功
- `-1`: 失败

**示例**:
```c
safety_stop();
```

---

### radar_get_status()

获取雷达状态。

```c
int radar_get_status(radar_result_t *result);
```

**参数**:
- `result`: 雷达数据输出

**雷达状态**:
| 状态 | 说明 |
|------|------|
| `RADAR_STATE_NO_TARGET` | 无目标 |
| `RADAR_STATE_MOVING` | 目标移动中 |
| `RADAR_STATE_STATIONARY` | 目标静止 |

**返回值**:
- `0`: 成功
- `-1`: 失败

**示例**:
```c
radar_result_t radar;
radar_get_status(&radar);

if (radar.state == RADAR_STATE_MOVING) {
    printf("Target detected at %d cm\n", radar.distance_cm);
}
```

---

### safety_is_anti_pinch_triggered()

检查是否触发防夹保护。

```c
bool safety_is_anti_pinch_triggered(void);
```

**返回值**:
- `true`: 触发防夹保护
- `false`: 未触发

**示例**:
```c
if (safety_is_anti_pinch_triggered()) {
    printf("Anti-pinch protection activated!\n");
}
```

---

### safety_reset()

重置防夹检测状态。

```c
void safety_reset(void);
```

**示例**:
```c
safety_reset();
```

---

## ⚙️ 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `anti_pinch_threshold` | 3 | 连续检测次数阈值 |
| `check_interval_ms` | 20 | 检测间隔（毫秒） |
| `detection_range_cm` | 100 | 检测范围（厘米） |

---

## 🚨 紧急响应流程

当触发防夹保护时，系统会执行以下操作：

1. **立即停止电机**
   ```c
   motor_control(MOTOR_CMD_EMERGENCY_STOP);
   ```

2. **反向开门**
   ```c
   usleep(100000);  /* 等待100ms */
   motor_control(MOTOR_CMD_OPEN);
   ```

3. **触发蜂鸣器报警**
   ```c
   buzzer_alarm(500);
   ```

4. **等待目标离开**
   - 持续监测雷达数据
   - 目标离开后恢复正常

---

## ⚠️ 注意事项

1. **实时性要求**: 检测间隔必须足够短（建议20ms）
2. **可靠性**: 防夹逻辑必须优先于其他操作
3. **误报处理**: 可以适当提高阈值减少误报
4. **测试验证**: 必须在实际硬件上充分测试

---

## 🔧 常见问题

### Q: 防夹功能误触发怎么办？
A: 可以尝试以下方法：
1. 增加检测阈值（从3改为5）
2. 减小检测范围
3. 检查雷达是否受干扰

### Q: 防夹功能不触发怎么办？
A: 检查以下几点：
1. 雷达是否正常工作
2. 检测范围是否合适
3. 检测间隔是否太长

### Q: 如何调整灵敏度？
A: 调整以下参数：
1. `anti_pinch_threshold`: 降低阈值提高灵敏度
2. `detection_range_cm`: 增大范围提高灵敏度
3. `check_interval_ms`: 减小间隔提高灵敏度

---

## 📝 使用示例

### 完整示例

```c
#include "safety.h"
#include "door_control.h"

int main(void)
{
    /* 初始化电机控制 */
    motor_config_t motor_config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };
    door_control_init(&motor_config);

    /* 初始化安全模块 */
    safety_init(NULL);

    /* 启动防夹检测 */
    safety_start();

    /* 开门 */
    motor_control(MOTOR_CMD_OPEN);
    sleep(5);
    motor_control(MOTOR_CMD_STOP);

    /* 关门（防夹检测会自动运行） */
    motor_control(MOTOR_CMD_CLOSE);
    sleep(10);

    /* 检查是否触发防夹 */
    if (safety_is_anti_pinch_triggered()) {
        printf("Anti-pinch was triggered!\n");
    }

    /* 停止检测 */
    safety_stop();

    /* 反初始化 */
    safety_deinit();
    door_control_deinit();

    return 0;
}
```

---

*最后更新: 2026-08-30*
