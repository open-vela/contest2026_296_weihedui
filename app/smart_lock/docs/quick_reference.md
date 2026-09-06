# UI 管理器快速参考卡片

## 核心 API

| 函数 | 说明 | 调用时机 |
|------|------|----------|
| `ui_manager_init()` | 初始化 UI 管理器 | 系统启动时调用一次 |
| `ui_manager_process()` | 处理 pending 队列 | 主循环中周期性调用（100Hz） |
| `ui_manager_deinit()` | 反初始化 UI 管理器 | 退出时调用（先停止主循环） |
| `ui_register_event_callback()` | 注册事件回调 | 初始化后调用 |

## 界面操作

| 函数 | 说明 |
|------|------|
| `ui_switch_screen(screen)` | 切换界面 |
| `ui_show_countdown(seconds)` | 显示倒计时 |
| `ui_cancel_countdown()` | 取消倒计时 |
| `ui_show_alarm(type)` | 显示报警 |
| `ui_close_alarm()` | 关闭报警 |

## 状态更新

| 函数 | 说明 |
|------|------|
| `ui_update_door_status(status)` | 更新门状态 |
| `ui_update_lock_status(status)` | 更新锁状态 |
| `ui_update_system_status(status)` | 更新系统状态 |

## 设置操作

| 函数 | 说明 |
|------|------|
| `ui_get_settings(settings)` | 获取设置参数 |
| `ui_set_settings(settings)` | 设置参数 |

## 事件类型

| 事件 | 说明 |
|------|------|
| `UI_EVENT_UNLOCK` | 开锁事件 |
| `UI_EVENT_LOCK` | 锁定事件 |
| `UI_EVENT_COUNTDOWN_END` | 倒计时结束 |
| `UI_EVENT_COUNTDOWN_CANCEL` | 取消倒计时 |
| `UI_EVENT_ALARM_CONFIRM` | 确认报警 |
| `UI_EVENT_SETTINGS_CHANGED` | 设置变更 |

## 界面 ID

| ID | 说明 |
|------|------|
| `UI_SCREEN_MAIN` | 主界面 |
| `UI_SCREEN_COUNTDOWN` | 倒计时界面 |
| `UI_SCREEN_ALERT` | 报警界面 |
| `UI_SCREEN_SETTINGS` | 设置界面 |

## 门状态

| 状态 | 说明 |
|------|------|
| `DOOR_STATUS_CLOSED` | 关闭 |
| `DOOR_STATUS_OPEN` | 打开 |
| `DOOR_STATUS_CLOSING` | 关闭中 |
| `DOOR_STATUS_OPENING` | 打开中 |

## 锁状态

| 状态 | 说明 |
|------|------|
| `LOCK_STATUS_LOCKED` | 已锁定 |
| `LOCK_STATUS_UNLOCKED` | 已解锁 |

## 报警类型

| 类型 | 说明 |
|------|------|
| `ALARM_ANTI_PINCH` | 防夹报警 |
| `ALARM_ABNORMAL_OPEN` | 异常开门 |
| `ALARM_LOW_BATTERY` | 低电量报警 |
| `ALARM_TAMPER` | 防撬报警 |

## 电池电量

| 电量 | 说明 |
|------|------|
| `BATTERY_LEVEL_EMPTY` | 空 |
| `BATTERY_LEVEL_1` | 1格 |
| `BATTERY_LEVEL_2` | 2格 |
| `BATTERY_LEVEL_3` | 3格 |
| `BATTERY_LEVEL_FULL` | 满电 |

## 典型代码模板

### 初始化

```c
#include "ui_manager.h"

void ui_event_handler(ui_event_t event, void *data, void *user_data)
{
    switch (event) {
        case UI_EVENT_UNLOCK: /* 开锁 */ break;
        case UI_EVENT_LOCK: /* 锁定 */ break;
        case UI_EVENT_COUNTDOWN_END: /* 倒计时结束 */ break;
        case UI_EVENT_COUNTDOWN_CANCEL: /* 取消倒计时 */ break;
        case UI_EVENT_ALARM_CONFIRM: /* 确认报警 */ break;
        case UI_EVENT_SETTINGS_CHANGED: /* 设置变更 */ break;
        default: break;
    }
}

int main(void)
{
    ui_manager_init();
    ui_register_event_callback(ui_event_handler, NULL);

    while (1) {
        ui_manager_process();
        lv_timer_handler();
        usleep(10000);
    }
}
```

### 开锁流程

```c
// 1. 更新锁状态
ui_update_lock_status(LOCK_STATUS_UNLOCKED);

// 2. 显示倒计时
ui_show_countdown(10);
```

### 报警流程

```c
// 1. 显示报警
ui_show_alarm(ALARM_ANTI_PINCH);

// 2. 等待用户确认（通过事件回调）
// UI_EVENT_ALARM_CONFIRM
```

### 设置流程

```c
// 1. 获取当前设置
ui_settings_t settings;
ui_get_settings(&settings);

// 2. 修改设置
settings.auto_close_time = 15;
settings.voice_volume = 90;

// 3. 应用设置
ui_set_settings(&settings);
```

## 编译配置

```makefile
# 源文件
CSRCS += app/smart_lock/src/ui_manager.c

# 头文件路径
CFLAGS += -I$(APPDIR)/app/smart_lock/include

# 依赖库
LDFLAGS += -lpthread
```

## NuttX 配置

```kconfig
CONFIG_PTHREAD=y
CONFIG_PTHREAD_MUTEX=y
CONFIG_PTHREAD_COND=y
CONFIG_LV_FONT_MONTSERRAT_24=y
CONFIG_LV_FONT_MONTSERRAT_36=y
CONFIG_LV_FONT_MONTSERRAT_48=y
```

## 注意事项

1. **主循环必须调用 `ui_manager_process()`**
2. **回调函数禁止 sleep、阻塞**
3. **退出时先停止主循环，再调用 `ui_manager_deinit()`**
4. **主循环频率推荐 100Hz（10ms 间隔）**

## 版本信息

- **版本**：v2.4.5
- **最后更新**：2024-01-01
