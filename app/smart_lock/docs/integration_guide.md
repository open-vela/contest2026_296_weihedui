# UI 管理器集成指南

## 1. 编译环境配置

### 1.1 NuttX 开启 Pthread

**必须打开**，否则 `pthread_mutex` / `pthread_cond` 编译报错。

在 NuttX 配置中启用：
```
CONFIG_PTHREAD=y
CONFIG_PTHREAD_MUTEX=y
CONFIG_PTHREAD_COND=y
```

配置方法：
```bash
# 使用 menuconfig
./tools/configure.sh <board_name>
make menuconfig

# 路程：RTOS Features → Pthread Support
# 或者直接修改 .config 文件
```

### 1.2 LVGL 字体配置

工程里面要有这几个字体定义，否则界面文字渲染异常（乱码/不显示）：

```c
extern lv_font_t lv_font_montserrat_24;
extern lv_font_t lv_font_montserrat_36;
extern lv_font_t lv_font_montserrat_48;
```

配置方法：

在 `lv_conf.h` 中启用字体：
```c
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1
```

或者在 LVGL 配置文件中启用：
```c
#define LV_FONT_CUSTOM_24 1
#define LV_FONT_CUSTOM_36 1
#define LV_FONT_CUSTOM_48 1
```

---

## 2. Makefile / CMake 配置

### 2.1 Makefile

```makefile
# 源文件列表
CSRCS += app/smart_lock/src/ui_manager.c

# 头文件路径
CFLAGS += -I$(APPDIR)/app/smart_lock/include

# 依赖库
LDFLAGS += -lpthread
```

### 2.2 CMake

```cmake
# 源文件列表
set(SOURCES
    ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_manager.c
)

# 头文件路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 依赖库
target_link_libraries(your_target pthread)
```

### 2.3 完整 Makefile 示例

```makefile
# apps/smart_lock/Makefile

include $(APPDIR)/Make.defs

# Smart Lock application
PROGNAME  = $(CONFIG_SMART_LOCK_PROGNAME)
PRIORITY  = $(CONFIG_SMART_LOCK_PRIORITY)
STACKSIZE = $(CONFIG_SMART_LOCK_STACKSIZE)
MODULE    = $(CONFIG_SMART_LOCK)

# 源文件
CSRCS     = src/ui_manager.c
MAINSRC   = src/main.c

# 头文件路径
CFLAGS   += ${INCDIR_PREFIX}$(APPDIR)/smart_lock/include

include $(APPDIR)/Application.mk
```

---

## 3. 业务层调用模板

### 3.1 UI 事件回调函数

```c
#include "ui_manager.h"

/**
 * @brief UI 事件回调函数
 * 
 * 注意：
 * - 只做消息转发！禁止 sleep、阻塞
 * - 快速返回，不要执行耗时操作
 * - 不要在回调中调用 ui_manager_deinit()
 */
void ui_event_handler(ui_event_t event, void *data, void *user_data)
{
    (void)data;
    (void)user_data;

    switch (event) {
        case UI_EVENT_UNLOCK:
            // 业务：开锁
            // motor_unlock();
            // voice_play(VOICE_DOOR_UNLOCKED);
            break;

        case UI_EVENT_LOCK:
            // 业务：锁定
            // motor_lock();
            // voice_play(VOICE_DOOR_LOCKED);
            break;

        case UI_EVENT_COUNTDOWN_END:
            // 倒计时结束：自动关门
            // motor_close_door();
            // voice_play(VOICE_DOOR_CLOSED);
            break;

        case UI_EVENT_COUNTDOWN_CANCEL:
            // 倒计时取消
            // voice_play(VOICE_CANCEL);
            break;

        case UI_EVENT_ALARM_CONFIRM:
            // 用户确认报警
            // alarm_stop();
            break;

        case UI_EVENT_SETTINGS_CHANGED: {
            // 设置参数变更，可以 ui_get_settings 读取
            int setting_type = (int)(intptr_t)data;
            ui_settings_t settings;
            ui_get_settings(&settings);

            switch (setting_type) {
                case 0:  // 自动关门时间
                    // motor_set_close_time(settings.auto_close_time);
                    break;
                case 1:  // 语音音量
                    // voice_set_volume(settings.voice_volume);
                    break;
                case 2:  // 屏幕亮度
                    // display_set_brightness(settings.screen_brightness);
                    break;
                case 3:  // 防夹功能
                    // sensor_set_antipinch(settings.antipinch_enabled);
                    break;
            }
            break;
        }

        default:
            break;
    }
}
```

### 3.2 系统初始化

```c
#include "ui_manager.h"

int main(void)
{
    // ... 其他初始化 ...

    // 初始化 UI 管理器
    int ret = ui_manager_init();
    if (ret < 0) {
        printf("UI manager init failed\n");
        return -1;
    }

    // 注册事件回调
    ui_register_event_callback(ui_event_handler, NULL);

    // 主循环
    while (1) {
        ui_manager_process();   // ⚠️ 关键！pending 队列、事件回调全部在这里跑
        lv_timer_handler();     // LVGL 定时器处理
        usleep(10000);          // 10ms，约 100Hz
    }

    // 不会走到这里
    return 0;
}
```

### 3.3 主循环频率说明

| 频率 | 间隔 | 适用场景 |
|------|------|----------|
| 100Hz | 10ms | 推荐，响应快 |
| 50Hz | 20ms | 可接受，CPU 占用低 |
| 200Hz | 5ms | 高响应，CPU 占用高 |

**推荐**：100Hz（10ms 间隔），平衡响应性和 CPU 占用。

---

## 4. 退出反初始化规则（非常重要）

### 4.1 错误做法

```c
// ❌ 错误：直接调用 deinit
while (1) {
    ui_manager_process();
    lv_timer_handler();
    usleep(10000);
}
ui_manager_deinit();  // 危险！可能崩溃
```

**问题**：
- `ui_manager_process()` 可能正在执行回调
- `ui_manager_deinit()` 会销毁 mutex
- 导致未定义行为，程序崩溃

### 4.2 正确做法

```c
// ✅ 正确：先停止主循环，再 deinit

// 1. 停止运行 ui_manager_process() 的任务/主循环
volatile bool running = true;

void signal_handler(int sig)
{
    running = false;
}

// 2. 等待主循环退出
while (running) {
    ui_manager_process();
    lv_timer_handler();
    usleep(10000);
}

// 3. 再调用 ui_manager_deinit()
ui_manager_deinit();
```

### 4.3 多线程环境

```c
// ✅ 多线程环境：join 等待线程退出

pthread_t ui_thread;
volatile bool ui_running = true;

void* ui_thread_func(void* arg)
{
    while (ui_running) {
        ui_manager_process();
        lv_timer_handler();
        usleep(10000);
    }
    return NULL;
}

// 启动线程
pthread_create(&ui_thread, NULL, ui_thread_func, NULL);

// ... 其他代码 ...

// 退出时：
// 1. 停止主循环
ui_running = false;

// 2. join 等待线程完全退出
pthread_join(ui_thread, NULL);

// 3. 再调用 deinit
ui_manager_deinit();
```

### 4.4 为什么必须这样做？

**ui_manager_deinit() 内部流程**：
1. 设置 `deinitializing = true`
2. 销毁资源（定时器、界面等）
3. 等待 `callback_refcount == 0`（使用 `pthread_cond_wait`）
4. 销毁条件变量
5. 销毁 mutex

**如果 ui_manager_process() 还在跑**：
- 可能正在执行回调（`callback_refcount > 0`）
- `ui_manager_deinit()` 会等待 `callback_refcount == 0`
- 但如果 `ui_manager_process()` 不再调用（因为主循环已停止），`callback_refcount` 永远不会变为 0
- 导致 `ui_manager_deinit()` 永久等待（死锁）

**正确流程**：
1. 先停止主循环（`ui_manager_process()` 不再被调用）
2. 等待当前正在执行的回调完成（`callback_refcount` 自然归零）
3. 再调用 `ui_manager_deinit()`，可以安全销毁

---

## 5. 完整示例代码

### 5.1 main.c

```c
#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "ui_manager.h"

/* 全局控制变量 */
static volatile bool g_running = true;

/* 信号处理函数 */
static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

/* UI 事件回调 */
static void ui_event_handler(ui_event_t event, void *data, void *user_data)
{
    (void)data;
    (void)user_data;

    switch (event) {
        case UI_EVENT_UNLOCK:
            printf("Unlock door\n");
            /* TODO: 调用开锁接口 */
            break;

        case UI_EVENT_LOCK:
            printf("Lock door\n");
            /* TODO: 调用锁定接口 */
            break;

        case UI_EVENT_COUNTDOWN_END:
            printf("Countdown end, closing door\n");
            /* TODO: 调用关门接口 */
            break;

        case UI_EVENT_COUNTDOWN_CANCEL:
            printf("Countdown cancelled\n");
            break;

        case UI_EVENT_ALARM_CONFIRM:
            printf("Alarm confirmed\n");
            /* TODO: 停止报警 */
            break;

        case UI_EVENT_SETTINGS_CHANGED:
            printf("Settings changed\n");
            break;

        default:
            break;
    }
}

/* 主函数 */
int main(int argc, char *argv[])
{
    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 初始化 UI 管理器 */
    if (ui_manager_init() < 0) {
        printf("UI manager init failed\n");
        return -1;
    }

    /* 注册事件回调 */
    ui_register_event_callback(ui_event_handler, NULL);

    printf("Smart lock started\n");

    /* 主循环 */
    while (g_running) {
        ui_manager_process();
        lv_timer_handler();
        usleep(10000);  /* 10ms */
    }

    /* 退出 */
    printf("Smart lock stopping...\n");
    ui_manager_deinit();
    printf("Smart lock stopped\n");

    return 0;
}
```

### 5.2 Kconfig

```kconfig
config SMART_LOCK
    bool "Smart Lock Application"
    default n
    depends on LVGL
    depends on PTHREAD
    ---help---
        Enable the Smart Lock application with LVGL UI.

if SMART_LOCK

config SMART_LOCK_PROGNAME
    string "Program name"
    default "smart_lock"

config SMART_LOCK_PRIORITY
    int "Smart Lock task priority"
    default 100

config SMART_LOCK_STACKSIZE
    int "Smart Lock stack size"
    default 8192

endif # SMART_LOCK
```

---

## 6. 常见问题

### Q1: 编译报错：undefined reference to `pthread_mutex_init`

**原因**：NuttX 未开启 Pthread 支持

**解决**：在配置中启用 `CONFIG_PTHREAD=y`

### Q2: 界面文字显示乱码或不显示

**原因**：LVGL 字体未启用

**解决**：在 `lv_conf.h` 中启用字体：
```c
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1
```

### Q3: 程序启动后立即崩溃

**原因**：`ui_manager_process()` 未在主循环中调用

**解决**：确保主循环中定期调用 `ui_manager_process()`

### Q4: 退出时程序卡死

**原因**：直接调用 `ui_manager_deinit()`，未停止主循环

**解决**：先停止主循环，再调用 `ui_manager_deinit()`

### Q5: 事件回调不执行

**原因**：`ui_manager_process()` 未被调用

**解决**：确保主循环中定期调用 `ui_manager_process()`

### Q6: 锁被占用，事件处理延迟

**原因**：其他线程持有锁，`ui_trylock()` 失败

**解决**：
- 减少其他线程持有锁的时间
- 或者接受少量延迟（设计取舍）

---

## 7. 调试技巧

### 7.1 启用日志

```c
/* 在 ui_manager.c 中修改日志级别 */
#define UI_LOG_LEVEL LOG_DEBUG  /* 改为 LOG_DEBUG */
```

### 7.2 监控 pending 队列

```c
/* 在 ui_manager_process() 中添加监控 */
int processed = ui_manager_process();
if (processed > 0) {
    printf("Processed %d events\n", processed);
}
```

### 7.3 监控引用计数

```c
/* 在 ui_manager_process() 中添加监控 */
if (g_ui_manager.callback_refcount > 0) {
    printf("Callback refcount: %d\n", g_ui_manager.callback_refcount);
}
```

### 7.4 检查锁状态

```c
/* 在关键点添加锁状态检查 */
printf("Lock state: %s\n", 
       pthread_mutex_trylock(&g_ui_manager.mutex) == 0 ? "unlocked" : "locked");
```

---

## 8. 性能优化

### 8.1 减少锁持有时间

```c
/* 避免在锁内执行耗时操作 */
ui_lock();
// 只做必要的状态更新
ui_unlock();
// 耗时操作放在锁外
```

### 8.2 减少回调执行时间

```c
/* 回调中只做消息转发 */
void ui_event_handler(ui_event_t event, void *data, void *user_data)
{
    /* 发送到消息队列，不在回调中直接处理 */
    msg_queue_send(event, data);
}
```

### 8.3 调整主循环频率

```c
/* 根据实际需求调整频率 */
#define UI_PROCESS_INTERVAL_MS  10  /* 100Hz */
// 或
#define UI_PROCESS_INTERVAL_MS  20  /* 50Hz */
```
