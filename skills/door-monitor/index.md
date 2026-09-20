# 门锁监控 Skill (door-monitor)

## Skill 定义

- **名称**: `door-monitor`
- **类型**: 阈值主动 + 事件主动
- **优先级**: 高
- **描述**: 基于雷达与门磁两路感知，在「门开着」且「雷达范围内无人」同时成立并持续到超时后，
  主动执行关门与落锁，无需用户指令。

选题方向为 **AI 硬件产品创新**，本 Skill 是该方向的核心：设备不等用户开口，
自己判断该不该动作。

## 触发条件

### 阈值主动触发

- **无人且门开**: `radar_check` 返回 `presence == false`，且 `door_sensor_read` 返回
  `door_state == "open"`，两者同时成立并持续 `autoclose` 秒后触发关门。
  `autoclose` 默认 10 秒，可在运行时用 `agent set_timeout <1-300>` 调整，无需重新烧录。

### 事件主动触发

- **门被打开**: 门磁由 `closed` 变为 `open`，开始计时。
- **雷达目标消失**: 雷达由检测到目标变为无目标，与上一条共同构成触发条件。

> 为什么要两路同时成立：只靠延时或只靠红外，会把还站在门口的人关在外面。
> 双传感器融合是本作品与「单纯延时关门」方案的主要差异。

## 执行逻辑

```
IDLE
  └─ 门磁 = open ──────────────► 计时中
        └─ 雷达 presence = false 且计时 ≥ autoclose
              └─ motor_control(close) ──► CLOSING
                    ├─ 关门到位 ──► motor_control(lock) ──► LOCKED
                    └─ 防夹触发 ──► EMERGENCY_STOP → OPEN（见 safety-alert）
```

状态机中 `MOTOR` 的取值（`IDLE` / `OPENING` / `CLOSING` / `LOCKING`）会实时反映在
板载 AMOLED 状态面板的 `MOTOR` 行上。

## 工具调用

本 Skill 通过设备已实现的 `agent` NSH 命令访问硬件能力，全部返回结构化 JSON：

| 工具 | 命令 | 返回 |
|---|---|---|
| 雷达检查 | `agent radar_check` | `{ "presence": bool, "distance": number, "state": "moving"\|"stationary" }` |
| 门磁读取 | `agent door_sensor_read` | `{ "closed": bool, "locked": bool, "door_state": string, "lock_state": string }` |
| 电机控制 | `agent motor_control <open\|close\|lock\|unlock\|stop>` | `{ "success": bool, "command": string }` |
| 超时设置 | `agent set_timeout <1-300>` | `{ "success": bool, "timeout": number }` |
| 状态汇总 | `agent system_status` | `{ "door": {...}, "radar": {...}, "motor": {...}, "ble": {...} }` |

设备能力以 JSON 暴露，是为了让**上层 AI 助手**可以直接调用，而不必解析自然语言。

## 运行时部署形态

```
/data/agent/skills/door-monitor/
├── index.md          # 本文件：触发条件与执行逻辑
└── config.yaml       # 阈值与开关
```

```yaml
# /data/agent/skills/door-monitor/config.yaml
name: door-monitor
version: 1.0.0
triggers:
  - type: threshold
    source: radar
    condition: no_presence_while_door_open
    timeout_source: autoclose      # 运行时可用 agent set_timeout 修改
  - type: event
    source: door_sensor
    event: door_opened
actions:
  - tool: motor_control
    when: "timeout_reached && !person_detected"
    params: { command: close }
  - tool: motor_control
    when: "door_closed_confirmed"
    params: { command: lock }
settings:
  autoclose_default: 10            # 与 Kconfig CONFIG_SMART_LOCK_AUTO_LOCK_TIMEOUT 一致
  anti_pinch_enabled: true
```

## 实现状态（如实说明）

- 触发条件所依赖的**门磁这一路当前不可用**：`src/door_sensor.c` 使用的是 Linux 的
  `/sys/class/gpio` 接口，而 NuttX 没有 `/sys`，`door_sensor_init()` 必然返回 `-2`。
  因此本 Skill 的自动关门链路在现有构建里**无法真正触发**，需先按
  `README.md` 第六节第 2 条把驱动改到 NuttX GPIO 字符驱动（`/dev/gpio0` + `GPIOIOC_*`）。
- 雷达这一路驱动是**可用的**（`/dev/ttyS0`，115200，HLK-LD2410 帧解析 + 校验和 +
  后台接收线程），但当前未接模组，故 `radar_check` 恒为无目标。
- `/data/agent/skills/` 的**运行时加载框架不在本作品的出厂镜像内**。本作品实机上提供的是
  `agent` NSH 命令（`src/agent_main.c`，本地分发到 `src/agent_tools.c` 的六个工具函数）。
  本文件描述的是该框架就绪后的 Skill 部署形态与触发场景。
