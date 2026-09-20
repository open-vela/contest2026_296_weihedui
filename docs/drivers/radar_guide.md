# HLK-LD2410 毫米波雷达驱动使用指南

## 1. 概述

HLK-LD2410 是一款 24GHz 毫米波雷达传感器，用于人体存在检测和运动检测。本驱动实现了 UART 数据帧解析功能，支持目标存在状态、运动状态、目标距离和能量值的提取。

## 2. 硬件连接

### 2.1 引脚定义

| 雷达引脚 | 开发板引脚 | 说明 |
|----------|-----------|------|
| VCC | 3.3V | 电源正极 |
| GND | GND | 电源地 |
| TX | UART RX | 数据发送 |
| RX | UART TX | 数据接收 |

### 2.2 连接示意图

```
HLK-LD2410          SF32LB52
+---------+         +---------+
| VCC     |---------| 3.3V    |
| GND     |---------| GND     |
| TX      |---------| UART RX |
| RX      |---------| UART TX |
+---------+         +---------+
```

## 3. 软件配置

### 3.1 UART 参数

| 参数 | 值 |
|------|-----|
| 设备路径 | /dev/ttyS0 |
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 流控 | 无 |

> **2026-09-19 实机修正**：黄山派上 UART1 已被 NSH 控制台占用（见下方 3.2 的
> `CONFIG_UART1_SERIAL_CONSOLE=y`），第二个 UART 实例注册为 `/dev/ttyS0`。
> 本表此前写的 `/dev/ttyS1` **在本板上并不存在**，实测 `radar_init()` 直接返回 `-2`（`ENOENT`）。
> 已改为 `/dev/ttyS0` 并重新烧录验证：串口可正常打开，帧解析与校验和逻辑完整。
> 详见 `src/radar_driver.c` 顶部注释与 `README.md` 第六节。

### 3.2 Kconfig 配置

在 `menuconfig` 中启用以下配置：

```
CONFIG_SMART_LOCK_RADAR=y
CONFIG_UART1_SERIAL_CONSOLE=y
```

## 4. API 说明

### 4.1 radar_init()

初始化雷达驱动，配置 UART 接口并创建接收线程。

**函数原型：**
```c
int radar_init(void);
```

**返回值：**
- `0`: 初始化成功
- `-EEXIST`: 驱动已初始化
- `-errno`: 其他错误

**使用示例：**
```c
int ret = radar_init();
if (ret < 0)
  {
    printf("Failed to initialize radar: %d\n", ret);
    return ret;
  }
```

### 4.2 radar_register_callback()

注册数据回调函数，当接收到新的雷达数据时会调用此回调。

**函数原型：**
```c
int radar_register_callback(radar_callback_t callback);
```

**参数：**
- `callback`: 回调函数指针，原型为 `void (*callback)(const radar_result_t *result)`

**返回值：**
- `0`: 注册成功
- `-ENODEV`: 驱动未初始化

**使用示例：**
```c
void my_radar_callback(const radar_result_t *result)
{
  printf("Target detected: %s\n",
         result->target_detected ? "YES" : "NO");
  printf("Motion state: %d\n", result->motion_state);
  printf("Distance: %d cm\n", result->target_distance);
}

radar_register_callback(my_radar_callback);
```

### 4.3 radar_get_status()

获取当前雷达状态。

**函数原型：**
```c
int radar_get_status(radar_result_t *result);
```

**参数：**
- `result`: 输出参数，存储雷达数据

**返回值：**
- `0`: 获取成功
- `-ENODEV`: 驱动未初始化
- `-EINVAL`: 参数无效

**使用示例：**
```c
radar_result_t result;
int ret = radar_get_status(&result);
if (ret == 0)
  {
    printf("Target detected: %s\n",
           result.target_detected ? "YES" : "NO");
    printf("Distance: %d cm\n", result.target_distance);
  }
```

### 4.4 radar_parse_frame()

手动解析雷达数据帧（用于测试或特殊场景）。

**函数原型：**
```c
int radar_parse_frame(const uint8_t *frame, size_t len);
```

**参数：**
- `frame`: 帧数据指针
- `len`: 帧数据长度

**返回值：**
- `0`: 解析成功
- `-EINVAL`: 参数无效或帧格式错误

### 4.5 radar_deinit()

反初始化雷达驱动，释放资源。

**函数原型：**
```c
int radar_deinit(void);
```

**返回值：**
- `0`: 反初始化成功

## 5. 数据结构

### 5.1 radar_result_t

雷达数据结果结构体。

```c
typedef struct
{
  bool     target_detected;    /* 是否检测到目标 */
  uint8_t  motion_state;       /* 运动状态 */
  uint16_t target_distance;    /* 目标距离 (cm) */
  uint8_t  energy_value;       /* 能量值 */
  uint32_t timestamp;          /* 时间戳 */
} radar_result_t;
```

### 5.2 运动状态定义

| 宏定义 | 值 | 说明 |
|--------|-----|------|
| RADAR_MOTION_NONE | 0x00 | 无目标 |
| RADAR_MOTION_STATIONARY | 0x01 | 静止目标 |
| RADAR_MOTION_MOVING | 0x02 | 运动目标 |

## 6. 帧格式说明

### 6.1 HLK-LD2410 帧格式

```
+--------+--------+--------+----------+--------+--------+
| 帧头   | 长度   | 功能码 | 数据区   | 校验和 | 帧尾   |
+--------+--------+--------+----------+--------+--------+
| 1 byte | 2 byte | 1 byte | N bytes  | 1 byte | 1 byte |
+--------+--------+--------+----------+--------+--------+
| 0xAA   | 小端序 | 0x01-  | 可变长度 | SUM    | 0x55   |
|        |        | 0x03   |          |        |        |
+--------+--------+--------+----------+--------+--------+
```

### 6.2 功能码说明

| 功能码 | 说明 | 数据长度 |
|--------|------|----------|
| 0x01 | 目标存在状态 | 1 byte |
| 0x02 | 运动状态 | 1 byte |
| 0x03 | 区域信息 | 4 bytes |

## 7. 使用示例

### 7.1 基本使用

```c
#include "radar_driver.h"
#include <stdio.h>
#include <unistd.h>

void radar_callback(const radar_result_t *result)
{
  printf("=== Radar Data ===\n");
  printf("Target: %s\n", result->target_detected ? "YES" : "NO");
  printf("Motion: %d\n", result->motion_state);
  printf("Distance: %d cm\n", result->target_distance);
  printf("Energy: %d\n", result->energy_value);
  printf("\n");
}

int main(void)
{
  int ret;

  /* 初始化雷达 */
  ret = radar_init();
  if (ret < 0)
    {
      printf("Failed to init radar: %d\n", ret);
      return -1;
    }

  /* 注册回调 */
  radar_register_callback(radar_callback);

  /* 主循环 */
  while (1)
    {
      sleep(1);
    }

  /* 反初始化 */
  radar_deinit();
  return 0;
}
```

### 7.2 轮询方式获取状态

```c
#include "radar_driver.h"
#include <stdio.h>
#include <unistd.h>

int main(void)
{
  radar_result_t result;
  int ret;

  ret = radar_init();
  if (ret < 0)
    {
      return -1;
    }

  while (1)
    {
      ret = radar_get_status(&result);
      if (ret == 0)
        {
          if (result.target_detected)
            {
              printf("Target detected at %d cm\n",
                     result.target_distance);
            }
        }
      usleep(100000);  /* 100ms */
    }

  radar_deinit();
  return 0;
}
```

## 8. 常见问题

### 8.1 无法接收数据

**可能原因：**
- UART 连接错误
- 波特率配置错误
- 设备路径错误

**解决方法：**
1. 检查硬件连接是否正确
2. 确认 UART 配置 (115200, 8N1)
3. 使用 `ls /dev/ttyS*` 确认设备路径

### 8.2 数据解析错误

**可能原因：**
- 帧格式不匹配
- 数据传输错误

**解决方法：**
1. 使用逻辑分析仪抓取 UART 数据
2. 检查帧头 (0xAA) 和帧尾 (0x55)
3. 验证校验和计算

### 8.3 检测距离不准

**可能原因：**
- 雷达安装位置不当
- 环境干扰
- 阈值参数需要调整

**解决方法：**
1. 调整雷达安装角度和高度
2. 远离金属物体和电磁干扰源
3. 参考 HLK-LD2410 数据手册调整参数

## 9. 参考资料

- [HLK-LD2410 数据手册](https://www.hlktech.com/)
- [NuttX UART 驱动文档](https://nuttx.apache.org/docs/latest/components/drivers/serial.html)
- [NuttX GPIO 驱动文档](https://nuttx.apache.org/docs/latest/components/drivers/character/gpio.html)
