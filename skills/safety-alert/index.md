# 安全告警 Skill (safety-alert)

## Skill 定义

- **名称**: `safety-alert`
- **类型**: 事件主动
- **优先级**: 最高（覆盖 door-monitor）
- **描述**: 在关门过程中持续监测门框区域是否有人，一旦检测到立即急停并反向释放，
  避免夹伤。本 Skill 的优先级高于自动关门，触发时无条件打断关门流程。

对应 `src/safety.c` 的实时监测任务：`smart_lock_main.c` 以 `check_interval_ms = 100`
启动该任务，任务内以更细的节奏轮询雷达状态。

## 触发条件

### 事件主动触发（最高优先级）

- **防夹触发**: 电机状态为 `MOTOR_STATE_CLOSING` 期间，雷达报告门框区域内有目标。
  两个条件**必须同时成立**——门没在关、或雷达没人，都不构成防夹事件。

> 只在 `CLOSING` 期间判定，是为了避免开门过程中有人经过门口就误报。
> 这是本 Skill 与「有人就报警」方案的差异。

### 阈值主动触发

- **门长时间开启**: 门打开超过设定时长仍未关闭（复用 `autoclose` 阈值，超时后由
  `door-monitor` 接管执行关门）。

## 执行逻辑

```
电机 = CLOSING
  └─ 雷达报告门框内有人 ──► 防夹触发
        ├─ motor_control(EMERGENCY_STOP)   # 立即急停
        ├─ motor_control(OPEN)             # 反向释放
        ├─ buzzer_alarm(500)               # 蜂鸣器报警
        └─ 状态面板横幅切至 ALARM / ANTI-PINCH / MOTOR STOPPED
```

触发后状态面板的顶栏配色由灰转**红**，大字号标题由 `NO DATA` 变为 `ALARM`，
`PINCH` 行由 `SAFE` 变为 `TRIGGERED`。这三个字段都可在实机上直接观察到
（见 `src/display.c` 的 `display_render()`，防夹分支享有最高优先级）。

## 工具调用

| 工具 | 命令 | 说明 |
|---|---|---|
| 雷达检查 | `agent radar_check` | 判断门框区域是否有目标 |
| 电机急停 | `agent motor_control stop` | 等价于触发时的 `EMERGENCY_STOP` |
| 反向释放 | `agent motor_control open` | 触发后释放门体 |
| 状态汇总 | `agent system_status` | 含 `motor.running` / `motor.direction`，可判断是否处于关门中 |

## 运行时部署形态

```
/data/agent/skills/safety-alert/
├── index.md
└── config.yaml
```

```yaml
# /data/agent/skills/safety-alert/config.yaml
name: safety-alert
version: 1.0.0
priority: highest          # 可打断 door-monitor 的关门动作
triggers:
  - type: event
    source: radar
    condition: target_in_doorway
    guard: "motor.state == CLOSING"     # 仅在关门过程中判定
actions:
  - tool: motor_control
    params: { command: emergency_stop }
  - tool: motor_control
    params: { command: open }
  - tool: buzzer
    params: { duration_ms: 500 }
settings:
  check_interval_ms: 100
```

## 实现状态（如实说明）

- 判定逻辑本身是**真实实现**：`src/safety.c` 直接调用 `radar_driver_get_status()` 取雷达
  数据（不是模拟数据），并与电机状态机联动。文件中残留的一处
  `radar_read_data()` 文档注释写着「模拟实现」是**过期注释**，函数体早已改为真实读取。
- **但触发动作落到的是桩函数**：`src/door_control.c` 的 `gpio_write()` 目前只打印
  `[MOTOR] GPIO %d -> HIGH/LOW` 日志、不驱动引脚（该文件顶部标注为「GPIO 模拟定义」），
  因此急停、反向释放、蜂鸣器都**不会产生真实物理动作**。电机状态机与防夹联动逻辑是完整的，
  可在实机上通过日志与状态面板完整观察，但需接上电机与蜂鸣器并填入真实引脚号后才有物理效果。
- 由于门磁当前不可用、雷达模组未接，本 Skill 在现有构建里**不会被自然触发**。若要在实机上
  观察其效果，可手动把电机驱动到 `CLOSING` 再让雷达报告近距目标。
- `/data/agent/skills/` 的运行时加载框架不在出厂镜像内，理由同 `door-monitor`。
