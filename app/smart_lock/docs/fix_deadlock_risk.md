# 跨锁风险修复说明

## 问题描述

在 `countdown_timer_cb` 定时器回调中，原代码存在跨锁风险：

```c
static void countdown_timer_cb(lv_timer_t *timer)
{
    ui_trylock();
    
    if (g_ui_manager.countdown_seconds <= 0) {
        lv_timer_del(g_ui_manager.countdown_timer);
        g_ui_manager.countdown_timer = NULL;
        
        ui_unlock();  // ⚠️ 释放锁
        
        // 风险窗口：其他线程可能在此期间修改 UI 状态
        
        notify_ui_event(UI_EVENT_COUNTDOWN_END, NULL);
        
        ui_switch_screen(UI_SCREEN_MAIN);  // ⚠️ 重新获取锁，可能与被修改的状态冲突
        return;
    }
    
    ui_unlock();
}
```

**风险分析**：
1. 释放锁后、重新获取锁前存在时间窗口
2. 其他线程可能在这个窗口内修改 UI 状态
3. 导致状态不一致或竞态条件

## 修复方案

使用**标志位延迟处理**，避免在定时器回调中直接切换界面：

### 核心思想

1. 定时器回调只设置待执行动作的标志位
2. 主循环中检查并处理这些待执行动作
3. 所有 UI 操作都在主循环的锁保护下执行

### 架构图

```
定时器回调（LVGL 线程）          主循环（用户线程）
        │                              │
        ▼                              ▼
   ┌─────────┐                   ┌─────────┐
   │ trylock │                   │ trylock │
   └────┬────┘                   └────┬────┘
        │                              │
        ▼                              ▼
   ┌─────────────┐               ┌─────────────┐
   │ 设置标志位  │               │ 检查标志位  │
   │ pending_    │               │ pending_    │
   │ action      │               │ action      │
   └──────┬──────┘               └──────┬──────┘
        │                              │
        ▼                              ▼
   ┌─────────┐                   ┌─────────────┐
   │ unlock  │                   │ 执行动作    │
   └─────────┘                   │ 切换界面    │
                                 │ 通知事件    │
                                 └──────┬──────┘
                                        │
                                        ▼
                                 ┌─────────┐
                                 │ unlock  │
                                 └─────────┘
```

## 代码实现

### 1. 添加待执行动作结构体

```c
typedef struct {
    ui_event_t event;           /* 事件类型 */
    void *data;                 /* 事件数据 */
    ui_screen_t target_screen;  /* 目标界面 */
    bool switch_screen;         /* 是否需要切换界面 */
    bool valid;                 /* 是否有效 */
} pending_action_t;
```

### 2. 在管理器中添加待执行动作

```c
typedef struct {
    // ... 其他字段 ...
    pending_action_t pending_action;
    bool has_pending_action;
    // ...
} ui_manager_t;
```

### 3. 设置待执行动作（定时器回调中调用）

```c
static void set_pending_action(ui_event_t event, void *data, 
                               ui_screen_t target, bool do_switch)
{
    g_ui_manager.pending_action.event = event;
    g_ui_manager.pending_action.data = data;
    g_ui_manager.pending_action.target_screen = target;
    g_ui_manager.pending_action.switch_screen = do_switch;
    g_ui_manager.pending_action.valid = true;
}
```

### 4. 处理待执行动作（主循环中调用）

```c
static int process_pending_action(void)
{
    if (!g_ui_manager.pending_action.valid) {
        return 0;
    }

    pending_action_t action = g_ui_manager.pending_action;
    g_ui_manager.pending_action.valid = false;

    /* 先切换界面 */
    if (action.switch_screen && action.target_screen < UI_SCREEN_MAX) {
        ui_switch_screen_internal(action.target_screen);
    }

    /* 再通知事件 */
    if (action.event != UI_EVENT_NONE) {
        notify_ui_event(action.event, action.data);
    }

    return 0;
}
```

### 5. 修改定时器回调

```c
static void countdown_timer_cb(lv_timer_t *timer)
{
    if (ui_trylock() != 0) return;
    if (g_ui_manager.deinitializing) { ui_unlock(); return; }

    g_ui_manager.countdown_seconds--;

    if (g_ui_manager.countdown_seconds <= 0) {
        if (g_ui_manager.countdown_timer) {
            lv_timer_del(g_ui_manager.countdown_timer);
            g_ui_manager.countdown_timer = NULL;
        }
        
        /* 修复：设置待执行动作，不在回调中直接切换界面 */
        set_pending_action(UI_EVENT_COUNTDOWN_END, NULL, UI_SCREEN_MAIN, true);
        
        ui_unlock();
        return;  /* 立即返回，不切换界面 */
    }

    /* 更新显示... */
    ui_unlock();
}
```

### 6. 添加主循环处理函数

```c
int ui_manager_process(void)
{
    if (!g_ui_manager.initialized) return -1;
    
    /* 使用 trylock 避免阻塞主循环 */
    if (ui_trylock() != 0) return 0;
    
    int ret = process_pending_action();
    
    ui_unlock();
    return ret;
}
```

### 7. 用户在主循环中调用

```c
int main(void)
{
    ui_manager_init();
    
    while (1) {
        /* 处理 UI 待执行动作 */
        ui_manager_process();
        
        /* LVGL 任务处理 */
        lv_task_handler();
        
        usleep(10000);  /* 10ms */
    }
    
    ui_manager_deinit();
    return 0;
}
```

## 修复对比

### 修复前（有跨锁风险）

```c
static void countdown_timer_cb(lv_timer_t *timer)
{
    ui_trylock();
    
    if (countdown <= 0) {
        lv_timer_del(timer);
        
        ui_unlock();                    // 释放锁
        // ⚠️ 风险窗口
        notify_ui_event(...);           // 可能与被修改的状态冲突
        ui_switch_screen(MAIN);         // 重新获取锁
        return;
    }
    
    ui_unlock();
}
```

### 修复后（无跨锁风险）

```c
static void countdown_timer_cb(lv_timer_t *timer)
{
    ui_trylock();
    
    if (countdown <= 0) {
        lv_timer_del(timer);
        
        set_pending_action(...);        // 只设置标志
        ui_unlock();                    // 释放锁
        return;                         // 立即返回
    }
    
    ui_unlock();
}

int ui_manager_process(void)
{
    ui_trylock();
    process_pending_action();           // 在锁保护下执行
    ui_unlock();
}
```

## 优点

1. **消除跨锁风险**：定时器回调中不直接切换界面
2. **简化锁管理**：所有 UI 操作都在主循环的锁保护下
3. **提高可预测性**：界面切换时机可控
4. **易于调试**：待执行动作可追踪

## 注意事项

1. **主循环必须定期调用 `ui_manager_process()`**
   - 建议每 10ms 调用一次
   - 使用 `trylock` 避免阻塞

2. **待执行动作只保留最新一个**
   - 如果有多个动作待执行，只保留最后一个
   - 适用于倒计时结束、报警确认等场景

3. **事件通知顺序**
   - 先切换界面，再通知事件
   - 确保上层业务在正确的界面状态下处理事件

## 测试建议

1. **功能测试**
   - 倒计时结束自动切换到主界面
   - 取消倒计时切换到主界面
   - 报警确认切换到主界面

2. **并发测试**
   - 多线程同时触发界面切换
   - 检查是否有状态不一致

3. **压力测试**
   - 快速连续触发倒计时
   - 检查是否有内存泄漏或死锁
