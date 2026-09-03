# AI Agent工具使用指南

## 1. 概述

AI Agent工具用于与智锁卫士设备进行交互，提供以下功能：
- 雷达检测
- 门磁状态读取
- 电机控制
- 超时设置
- 系统状态查询

## 2. 工具列表

### 2.1 雷达检查工具

**函数**: `tool_radar_check()`

**功能**: 获取雷达检测状态

**返回格式**:
```json
{
    "presence": true,
    "zone": "indoor",
    "distance": 150,
    "energy": 85,
    "state": "moving"
}
```

**参数说明**:
- `presence`: 是否检测到目标 (true/false)
- `zone`: 目标所在区域 (indoor/outdoor)
- `distance`: 目标距离 (厘米)
- `energy`: 能量值 (0-100)
- `state`: 目标状态 (moving/stationary)

**使用示例**:
```bash
# 调用雷达检查
agent radar_check
```

### 2.2 门磁检查工具

**函数**: `tool_door_sensor_read()`

**功能**: 获取门磁传感器状态

**返回格式**:
```json
{
    "closed": true,
    "locked": true,
    "door_state": "closed",
    "lock_state": "locked"
}
```

**参数说明**:
- `closed`: 门是否关闭 (true/false)
- `locked`: 门是否锁定 (true/false)
- `door_state`: 门状态 (closed/open)
- `lock_state`: 锁状态 (locked/unlocked)

**使用示例**:
```bash
# 调用门磁检查
agent door_sensor_read
```

### 2.3 电机控制工具

**函数**: `tool_motor_control(argc, argv)`

**功能**: 控制电机执行开关门操作

**参数**:
- `argv[1]`: 控制命令
  - `open`: 开门
  - `close`: 关门
  - `lock`: 锁门
  - `unlock`: 解锁
  - `stop`: 停止

**返回格式**:
```json
{
    "success": true,
    "command": "open"
}
```

**使用示例**:
```bash
# 开门
agent motor_control open

# 关门
agent motor_control close

# 锁门
agent motor_control lock

# 解锁
agent motor_control unlock

# 停止
agent motor_control stop
```

### 2.4 超时设置工具

**函数**: `tool_set_timeout(argc, argv)`

**功能**: 设置自动关门超时时间

**参数**:
- `argv[1]`: 超时时间（秒），范围1-300

**返回格式**:
```json
{
    "success": true,
    "timeout": 30
}
```

**使用示例**:
```bash
# 设置30秒超时
agent set_timeout 30

# 设置60秒超时
agent set_timeout 60
```

### 2.5 系统状态工具

**函数**: `tool_system_status(argc, argv)`

**功能**: 获取完整的系统状态

**返回格式**:
```json
{
    "door": {
        "state": "closed",
        "locked": true
    },
    "radar": {
        "presence": true,
        "distance": 150,
        "state": "moving"
    },
    "motor": {
        "running": false,
        "direction": "opening"
    },
    "ble": {
        "connected": true
    }
}
```

**使用示例**:
```bash
# 获取系统状态
agent system_status
```

## 3. 集成指南

### 3.1 在代码中调用

```c
#include "smart_lock.h"

// 检查雷达状态
radar_result_t result;
radar_get_status(&result);

// 读取门状态
door_status_t status;
door_sensor_get_status(&status);

// 控制电机
motor_control(MOTOR_CMD_OPEN);

// 设置超时
set_auto_close_timeout(30);
```

### 3.2 JSON输出格式

所有工具都使用标准JSON格式输出，便于解析：

```c
// 打印JSON格式
printf("{\"key\":\"value\"}\n");

// 布尔值
printf("{\"flag\":%s}\n", flag ? "true" : "false");

// 数值
printf("{\"count\":%d}\n", count);
```

## 4. 错误处理

### 4.1 常见错误

| 错误信息 | 原因 | 解决方案 |
|---|---|---|
| `missing command` | 缺少必要参数 | 检查命令格式 |
| `invalid command` | 无效的命令 | 使用有效命令 |
| `radar read failed` | 雷达读取失败 | 检查雷达连接 |
| `door sensor read failed` | 门磁读取失败 | 检查门磁连接 |

### 4.2 错误返回格式

```json
{
    "error": "错误描述"
}
```

## 5. 调试技巧

### 5.1 查看日志

```bash
# 查看Agent相关日志
grep "Agent:" logs/smart_lock.log

# 查看工具调用日志
grep "tool_" logs/smart_lock.log
```

### 5.2 测试工具

```bash
# 测试雷达检查
agent radar_check

# 测试门磁检查
agent door_sensor_read

# 测试电机控制
agent motor_control open
```

## 6. 最佳实践

### 6.1 参数验证
- 始终检查参数数量和有效性
- 提供清晰的错误信息
- 返回标准JSON格式

### 6.2 状态管理
- 定期查询系统状态
- 处理状态变化通知
- 实现重试机制

### 6.3 安全考虑
- 验证控制命令来源
- 实现操作确认机制
- 记录操作日志

## 7. 参考资料

- [AI Agent架构文档](docs/architecture/agent.md)
- [传感器接口文档](docs/hardware/sensors.md)
- [电机控制文档](docs/hardware/motor.md)
