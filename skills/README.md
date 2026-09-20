# 智锁卫士 — AI Agent Skills

本目录沉淀本作品的 **AI Agent Skill 定义**（大赛硬性要求：至少沉淀 1 个）。

设备把硬件能力通过 `agent` NSH 命令暴露成结构化 JSON，
Skill 定义的是「在什么条件下、调哪些工具、按什么顺序」的主动行为策略。

| Skill | 类型 | 优先级 | 说明 | 对应实现 |
|---|---|---|---|---|
| [`door-monitor`](door-monitor/index.md) | 阈值主动 + 事件主动 | 高 | 雷达无人且门开着，超时后自动关门落锁 | `src/smart_lock_main.c` 主循环 |
| [`safety-alert`](safety-alert/index.md) | 事件主动 | 最高 | 关门过程中检测到人立即急停并反向释放 | `src/safety.c` 实时监测任务 |

## 为什么只有这两个

本地设计阶段另有两份 Skill 草稿（`smart-home` 智能家居联动、`daily-report` 每日报告），
**未纳入本目录**：它们分别依赖 BLE Mesh/Matter 联动与手机端推送，
而这两条通路在本作品的出厂构建里都不存在（BLE 见 `README.md` 第六节第 3 条）。
把不存在的链路写进 Skill 会让描述与代码脱节，故不收录。

## 运行时部署形态

```
/data/agent/skills/
├── door-monitor/{index.md, config.yaml}
└── safety-alert/{index.md, config.yaml}
```

`config.yaml` 给出阈值与开关。`door-monitor` 的 `autoclose` 阈值可在运行时用
`agent set_timeout <1-300>` 修改，无需重新烧录。

> **实现状态**：`/data/agent/skills/` 的运行时加载框架**不在本作品的出厂镜像内**。
> 实机上提供的是 `agent` NSH 命令（`src/agent_main.c` → `src/agent_tools.c` 的六个工具函数）。
> 本目录描述的是该框架就绪后的 Skill 部署形态与触发场景，各 Skill 文末均注明当前可触发状态。
