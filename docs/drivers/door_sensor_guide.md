# 门磁传感器驱动使用指南

## 1. 概述

门磁传感器用于检测门的开闭状态。本驱动实现了 GPIO 中断监测功能，支持门状态检测、消抖处理和状态变化回调通知。

## 2. 硬件连接

### 2.1 引脚定义

| 门磁引脚 | 开发板引脚 | 说明 |
|----------|-----------|------|
| VCC | 3.3V | 电源正极 |
| GND | GND | 电源地 |
| SIGNAL | GPIO | 信号输出 |

### 2.2 连接示意图

```
门磁传感器          SF32LB52
+---------+         +---------+
| VCC     |---------| 3.3V    |
| GND     |---------| GND     |
| SIGNAL  |---------| GPIO 0  |
+---------+         +---------+
```

### 2.3 安装注意事项

1. 门磁传感器应安装在门框和门扇上
2. 两部分间距应小于 15mm
3. 避免安装在金属门上（可能影响信号）
4. 确保传感器安装牢固

## 3. 软件配置

### 3.1 GPIO 配置

| 参数 | 值 |
|------|-----|
| GPIO 引脚 | 0 (可在头文件中修改) |
| 方向 | 输入模式 |
| 上拉 | 使能 |
| 中断触发 | 双边沿 |

### 3.2 Kconfig 配置

在 `menuconfig` 中启用以下配置：

```
CONFIG_SMART_LOCK_DOOR_SENSOR=y
CONFIG_GPIO=y
CONFIG_GPIO_INTERRUPT=y
```

## 4. API 说明

### 4.1 door_sensor_init()

初始化门磁传感器，配置 GPIO 中断并创建监控线程。

**函数原型：**
```c
int door_sensor_init(void);
```

**返回值：**
- `0`: 初始化成功
- `-EEXIST`: 驱动已初始化
- `-errno`: 其他错误

**使用示例：**
```c
int ret = door_sensor_init();
if (ret < 0)
  {
    printf("Failed to initialize door sensor: %d\n", ret);
    return ret;
  }
```

### 4.2 door_sensor_register_callback()

注册状态变化回调函数，当门状态或锁状态变化时会调用此回调。

**函数原型：**
```c
int door_sensor_register_callback(door_status_callback_t callback);
```

**参数：**
- `callback`: 回调函数指针，原型为 `void (*callback)(const door_status_t *status)`

**返回值：**
- `0`: 注册成功
- `-ENODEV`: 驱动未初始化

**使用示例：**
```c
void door_status_changed(const door_status_t *status)
{
  printf("Door state: %s\n",
         status->door_state == DOOR_STATE_CLOSED ?
         "CLOSED" : "OPEN");
  printf("Lock state: %s\n",
         status->lock_state == LOCK_STATE_LOCKED ?
         "LOCKED" : "UNLOCKED");
}

door_sensor_register_callback(door_status_changed);
```

### 4.3 door_sensor_get_status()

获取当前门状态。

**函数原型：**
```c
int door_sensor_get_status(door_status_t *status);
```

**参数：**
- `status`: 输出参数，存储门状态

**返回值：**
- `0`: 获取成功
- `-ENODEV`: 驱动未初始化
- `-EINVAL`: 参数无效

**使用示例：**
```c
door_status_t status;
int ret = door_sensor_get_status(&status);
if (ret == 0)
  {
    printf("Door: %s, Lock: %s\n",
           status.door_state == DOOR_STATE_CLOSED ?
           "CLOSED" : "OPEN",
           status.lock_state == LOCK_STATE_LOCKED ?
           "LOCKED" : "UNLOCKED");
  }
```

### 4.4 door_sensor_set_lock_state()

设置锁状态（由主程序调用，控制电子锁）。

**函数原型：**
```c
int door_sensor_set_lock_state(bool locked);
```

**参数：**
- `locked`: `true` 锁定, `false` 解锁

**返回值：**
- `0`: 设置成功
- `-ENODEV`: 驱动未初始化

**使用示例：**
```c
/* 锁门 */
door_sensor_set_lock_state(true);

/* 解锁 */
door_sensor_set_lock_state(false);
```

### 4.5 door_sensor_deinit()

反初始化门磁传感器，释放资源。

**函数原型：**
```c
int door_sensor_deinit(void);
```

**返回值：**
- `0`: 反初始化成功

## 5. 数据结构

### 5.1 door_status_t

门磁状态结构体。

```c
typedef struct
{
  door_state_t  door_state;    /* 门状态 */
  lock_state_t  lock_state;    /* 锁状态 */
  time_t        timestamp;     /* 状态变化时间戳 */
} door_status_t;
```

### 5.2 door_state_t

门状态枚举。

| 枚举值 | 值 | 说明 |
|--------|-----|------|
| DOOR_STATE_UNKNOWN | 0 | 未知状态 |
| DOOR_STATE_OPEN | 1 | 门打开 |
| DOOR_STATE_CLOSED | 2 | 门关闭 |

### 5.3 lock_state_t

锁状态枚举。

| 枚举值 | 值 | 说明 |
|--------|-----|------|
| LOCK_STATE_UNKNOWN | 0 | 未知状态 |
| LOCK_STATE_UNLOCKED | 1 | 未锁定 |
| LOCK_STATE_LOCKED | 2 | 已锁定 |

## 6. 消抖处理

驱动内置了 50ms 的消抖处理，防止因机械抖动导致的误触发。

### 6.1 消抖原理

```
 GPIO 信号
    │
    ▼
┌───────────────────────┐
│  检测到边沿中断        │
└───────────────────────┘
    │
    ▼
┌───────────────────────┐
│  记录当前时间戳        │
└───────────────────────┘
    │
    ▼
┌───────────────────────┐
│  与上次中断时间比较    │
│  < 50ms? 忽略中断     │
│  ≥ 50ms? 处理中断     │
└───────────────────────┘
```

## 7. 使用示例

### 7.1 基本使用

```c
#include "door_sensor.h"
#include <stdio.h>
#include <unistd.h>

void door_callback(const door_status_t *status)
{
  time_t now = time(NULL);
  printf("[%s] Door: %s, Lock: %s\n",
         ctime(&now),
         status->door_state == DOOR_STATE_CLOSED ?
         "CLOSED" : "OPEN",
         status->lock_state == LOCK_STATE_LOCKED ?
         "LOCKED" : "UNLOCKED");
}

int main(void)
{
  int ret;

  /* 初始化门磁传感器 */
  ret = door_sensor_init();
  if (ret < 0)
    {
      printf("Failed to init door sensor: %d\n", ret);
      return -1;
    }

  /* 注册回调 */
  door_sensor_register_callback(door_callback);

  /* 主循环 */
  while (1)
    {
      sleep(1);
    }

  /* 反初始化 */
  door_sensor_deinit();
  return 0;
}
```

### 7.2 配合锁控制使用

```c
#include "door_sensor.h"
#include <stdio.h>
#include <unistd.h>

int main(void)
{
  door_status_t status;
  int ret;

  ret = door_sensor_init();
  if (ret < 0)
    {
      return -1;
    }

  while (1)
    {
      ret = door_sensor_get_status(&status);
      if (ret == 0)
        {
          /* 门打开超过 10 秒，自动锁门 */
          if (status.door_state == DOOR_STATE_OPEN &&
              (time(NULL) - status.timestamp) > 10)
            {
              printf("Auto-locking door...\n");
              door_sensor_set_lock_state(true);
            }
        }
      sleep(1);
    }

  door_sensor_deinit();
  return 0;
}
```

## 8. 常见问题

### 8.1 无法检测到门状态变化

**可能原因：**
- GPIO 连接错误
- 门磁传感器安装不当
- 中断配置错误

**解决方法：**
1. 检查硬件连接是否正确
2. 确认门磁传感器安装位置和间距
3. 使用 `cat /sys/class/gpio/gpio0/value` 手动读取 GPIO 值

### 8.2 状态频繁抖动

**可能原因：**
- 门磁传感器安装松动
- 消抖时间设置不当

**解决方法：**
1. 重新固定门磁传感器
2. 调整 `DEBOUNCE_TIME_MS` 参数（在头文件中）

### 8.3 中断不触发

**可能原因：**
- GPIO 中断未使能
- 边沿触发方式配置错误

**解决方法：**
1. 确认 `CONFIG_GPIO_INTERRUPT=y` 已启用
2. 检查 `/sys/class/gpio/gpio0/edge` 是否为 "both"
3. 确认 GPIO 引脚号正确

## 9. 参考资料

- [NuttX GPIO 驱动文档](https://nuttx.apache.org/docs/latest/components/drivers/character/gpio.html)
- [NuttX 中断处理](https://nuttx.apache.org/docs/latest/kernel/external_interrupts.html)
- [门磁传感器选购指南](https://www.example.com)
