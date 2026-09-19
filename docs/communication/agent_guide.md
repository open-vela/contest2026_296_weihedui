# AI Agent工具使用指南

## 1. 概述

AI Agent工具用于与智锁卫士设备进行交互，提供以下功能：
- 雷达检测
- 门磁状态读取
- 电机控制
- 超时设置
- 系统状态查询
- 屏幕亮度调节

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


### 2.6 屏幕亮度工具

**函数**: `tool_brightness(argc, argv)`

**功能**: 调节板载 1.85" AMOLED 的发光亮度

**参数**:
- `argv[1]`: 可选，亮度百分比，范围 0-100
- `argv[2]`: 可选，字面量 `force`，跳过自检强制写入（诊断用）

不带参数时只查询，不写面板。`diag` 只读转储寄存器，不改任何硬件状态。

**返回格式**（查询）:
```json
{
    "available": true,
    "verified": true,
    "panel_id": "0x331100",
    "percent": 50,
    "readback": 127,
    "readback_note": "0x52 mirrors 0x51; equals raw_written after a set"
}
```

**返回格式**（设置）:
```json
{
    "success": true,
    "percent": 30,
    "raw_written": 76,
    "forced": false
}
```

> **为什么 set 响应没有 `readback`：** CO5300 的 DDIC 寄存器更新有微小延迟
> （约一帧），写完立刻从 0x52 读拿到的是旧值。因此把回读留给显式的查询路径
> （无参数 `agent brightness`），那时延迟已过、回读是准的。上板实测：
> `brightness 30` 即时回读 127（旧值）→ 3 秒后查询回读 76（正确）。

**返回格式**（`diag`）:
```json
{
    "panel_id": "0x331100",
    "power_mode": "0x9C",
    "madctl": "0x00",
    "colmod": "0x55",
    "wbright": "0x7F",
    "rbright": "0x7F",
    "wrctrld": "0x20",
    "wrhbmdl": "0xFF"
}
```

**参数说明**:
- `available`: LCDC 句柄是否已取得 (true/false)
- `verified`: 自检是否通过，即读面板 ID(0x04) 得到 0x331100 (true/false)。
  未通过时默认拒绝写入
- `panel_id`: 面板 ID，本板应为 `0x331100`；`null` 表示读失败
- `percent`: 亮度百分比；`null` 表示本次上电后未成功设置过
- `raw_written`: 实际写进 0x51 的原始值 (0-255)
- `readback`: 写完后立刻回读 0x52 的结果。正常情况下**等于 `raw_written`**
  （见下）；`-1` 表示回读失败
- `forced`: 本次是否使用了 force 模式

**使用示例**:
```bash
# 查询当前亮度
agent brightness

# 调到 30%
agent brightness 30

# 调到最亮
agent brightness 100

# 自检未通过时强制写入（诊断）
agent brightness 50 force

# 只读转储面板寄存器
agent brightness diag
```

**说明**:

AMOLED 是自发光器件，模组上没有背光电路，22p QSPI FPC 上也没有 BL_PWM
引脚，因此"调光"不能像 LCD 那样调背光 PWM，只能写面板自己的亮度寄存器
CO5300 的 0x51（Write Display Brightness）。驱动初始化时把 0x51 写成
0x7F（127/255 ≈ 50%），这也是 `percent` 的默认值 50 的来源。

> 引脚层面的依据见 `docs/hardware/display_lcm.md`：22p QSPI FPC 接口定义（照抄
> 思澈 wiki 原文）中确实没有 BL_PWM 这一脚，它只出现在 40p RGB FPC 接口上。
> 思澈 wiki 的《AMOLED屏背光电路》一节也写明 AMOLED「没有像TFT屏一样的背光源……
> 所以无需专门提供PWM信号来调整背光亮度」，与本模块的实现思路一致。

启动时 `smart_lock_main()` 会先做一次自检：读面板 ID(0x04)，期待得到
0x331100。自检通过才允许普通写入；未通过时 `agent brightness <n>` 返回
`{"success":false,...}`，需要加 `force` 才能写入。

**关于 `readback`：本面板的 0x52 是准的，可以直接当判据。**

CO5300 实现了 RDDISBV（0x52），读回来的就是 0x51 写进去的值。实测闭环：

| 操作 | `raw_written` | diag `wbright` | diag `rbright` |
|---|---|---|---|
| `brightness 100` | 255 | `0xFF` | `0xFF` |
| `brightness 5` | 12 | `0x0C` | `0x0C` |
| 开机后未写过 | — | `0x7F` | `0x7F` |

全部逐一对上。所以：

- **寄存器层面**：`readback == raw_written` 即证明这笔写真的落到了面板上；
- **视觉层面**：已由人眼确认亮度确实随写入值变化（100% → 5% 明显变暗，
  → 100% 恢复），肉眼和寄存器双端闭环成立；
- 显示纯黑时 AMOLED 不发光，因此测试前要让屏幕上有内容
  （例如先跑 `smart_lock &`），否则任何亮度值看上去都一样。

> 记录一笔反复：本模块早期的单次读实现读 0x52 稳定得到 0x00，当时据此
> 写过"这颗 DDIC 没有实现 RDDISBV"的结论，**那个结论是错的**。后来读
> 改为失败重试、且自检先读一次 0x04，0x00 再没复现。两处改动是一起进去
> 的，没做单变量对照，因此无法断定是重试解决的还是首次读需要"热身"。

**另外两条独立证据**：

1. 句柄是真的 —— 自检能从 0x04 读出 0x331100，用的正是
   `board_lcd_getdev(0)` + `sizeof(struct lcd_dev_s)` 算出的那个句柄；
2. 写通路是真的 —— 驱动 init 用同一条序列写了 `COLMOD(0x3A)←0x55`、
   `WRCTRLD(0x53)←0x20`、`WRHBMDISBV(0x63)←0xFF`，而屏幕确实以 RGB565
   正常显示，这些写必须落到面板上屏才会亮。`agent brightness diag` 能把
   这几笔厂商写的值全部读回来核对（正常应为 `0x55` / `0x20` / `0xFF`）。

**为什么每次写亮度都要同时写 0x53：**

实测发现面板寄存器会被持续重置——colmod 从 `0x55` 回到 `0x77`（复位默认值），
wbright 从 `0x7F` 回到 `0x00`，说明有后台初始化路径在反复运行。如果只写
0x51（亮度值），0x53 的 BL bit 可能已被清掉，面板会忽略 0x51 的新值。

因此 `panel_brightness_force()` 的写序列是：
1. **先写 0x53 = 0x20**（bit5 = BL，使能亮度控制）；
2. **再写 0x51 = 目标值**。

顺序必须如此——0x51 在 0x53 之后，否则控制位的写入会覆盖亮度值。
上板验证：这个写法让所有寄存器保持稳定（diag 连续输出 `0x55`/`0x20`/`0xFF`）。

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

// 调节屏幕亮度（0-100）
panel_brightness_set(30);
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
| `invalid brightness (0-100)` | 亮度参数越界 | 传 0-100 之间的整数 |
| `self-test not passed, retry with 'force'` | 面板 ID 自检未通过 | 用 `agent brightness diag` 看 `panel_id`；确需写入时加 `force` |
| `panel write failed` | 亮度寄存器写入失败 | 检查显示是否正常刷新，稍后重试 |

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

# 测试屏幕亮度
agent brightness
agent brightness diag
agent brightness 20
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
