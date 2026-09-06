# v2.3 修复回调重入风险

## 问题描述

`notify_ui_event()` 在锁内调用外部回调，如果回调函数内部又调用 UI 管理器的 API，会导致重入问题。

```c
// 问题：在锁内调用回调
static void notify_ui_event(ui_event_t event, void *data)
{
    if (g_ui_manager.event_callback) {
        // ⚠️ 在锁内调用外部回调
        // 如果回调内部调用 ui_lock()，可能导致死锁
        g_ui_manager.event_callback(event, data, g_ui_manager.event_user_data);
    }
}
```

**重入场景**：
1. 用户点击开锁按钮
2. `unlock_btn_event_cb()` 获取锁
3. 调用 `notify_ui_event()`
4. 回调函数内部调用 `ui_update_lock_status()`
5. `ui_update_lock_status()` 尝试获取锁
6. 如果使用非递归锁 → 死锁
7. 如果使用递归锁 → 状态可能不一致

## 修复方案

### 方案 1：释放锁后调用回调（推荐）

```c
static void notify_ui_event(ui_event_t event, void *data)
{
    /* 保存回调指针（调用时已持有锁） */
    ui_event_cb_t cb = g_ui_manager.event_callback;
    void *user_data = g_ui_manager.event_user_data;

    /* 释放锁后再调用回调 */
    ui_unlock();

    if (cb) {
        cb(event, data, user_data);
    }

    /* 重新获取锁 */
    ui_lock();
}
```

### 方案 2：复制所有待执行动作后释放锁

```c
static int process_pending_action(void)
{
    pending_action_t actions[PENDING_ACTION_QUEUE_SIZE];
    int count = 0;

    /* 1. 复制所有待执行动作到临时数组（在锁内） */
    while (count < PENDING_ACTION_QUEUE_SIZE && !pending_queue_is_empty()) {
        if (pending_queue_pop(&actions[count]) == 0) {
            count++;
        } else {
            break;
        }
    }

    if (count == 0) {
        return 0;
    }

    /* 2. 执行界面切换（在锁内） */
    for (int i = 0; i < count; i++) {
        if (actions[i].switch_screen && actions[i].target_screen < UI_SCREEN_MAX) {
            ui_switch_screen_internal(actions[i].target_screen);
        }
    }

    /* 3. 释放锁后调用回调 */
    ui_unlock();

    for (int i = 0; i < count; i++) {
        if (actions[i].event != UI_EVENT_NONE && g_ui_manager.event_callback) {
            g_ui_manager.event_callback(actions[i].event, actions[i].data, g_ui_manager.event_user_data);
        }
    }

    /* 4. 重新获取锁 */
    ui_lock();

    return count;
}
```

## 修复对比

### 修复前（在锁内调用回调）

```c
static void notify_ui_event(ui_event_t event, void *data)
{
    if (g_ui_manager.event_callback) {
        g_ui_manager.event_callback(event, data, g_ui_manager.event_user_data);
        // ⚠️ 回调内部可能调用 UI API，导致重入
    }
}

static void unlock_btn_event_cb(lv_event_t *e)
{
    ui_lock();
    notify_ui_event(UI_EVENT_UNLOCK, NULL);  // 在锁内调用
    ui_unlock();
}
```

### 修复后（释放锁后调用回调）

```c
static void notify_ui_event(ui_event_t event, void *data)
{
    ui_event_cb_t cb = g_ui_manager.event_callback;
    void *user_data = g_ui_manager.event_user_data;

    ui_unlock();  // 释放锁

    if (cb) {
        cb(event, data, user_data);  // 在锁外调用
    }

    ui_lock();  // 重新获取锁
}

static void unlock_btn_event_cb(lv_event_t *e)
{
    ui_lock();
    notify_ui_event(UI_EVENT_UNLOCK, NULL);  // 会释放锁再获取锁
    ui_unlock();
}
```

## 调用流程

### unlock_btn_event_cb 调用流程

```
1. ui_lock()                    // 锁计数 = 1
2. notify_ui_event()
   2.1 保存回调指针
   2.2 ui_unlock()              // 锁计数 = 0 (释放锁)
   2.3 调用回调函数
   2.4 ui_lock()                // 锁计数 = 1 (重新获取锁)
3. ui_unlock()                  // 锁计数 = 0 (释放锁)
```

### process_pending_action 调用流程

```
1. ui_lock()                    // 锁计数 = 1
2. 复制所有待执行动作到临时数组
3. 执行界面切换
4. ui_unlock()                  // 锁计数 = 0 (释放锁)
5. 调用所有回调函数
6. ui_lock()                    // 锁计数 = 1 (重新获取锁)
7. 返回调用者
```

## 安全性分析

### 线程安全

- ✅ 回调在锁外调用，不会死锁
- ✅ 回调指针在锁内保存，不会失效
- ✅ 界面切换在锁内执行，状态一致

### 重入安全

- ✅ 回调内部可以调用 UI API
- ✅ 回调内部可以获取锁
- ✅ 不会导致死锁

### 状态一致性

- ✅ 界面切换在锁内执行
- ✅ 事件通知在锁外执行
- ✅ 回调返回后重新获取锁

## 测试场景

### 场景 1：回调内部调用 UI API

```c
void my_callback(ui_event_t event, void *data, void *user_data)
{
    if (event == UI_EVENT_UNLOCK) {
        // 在回调内部调用 UI API（安全）
        ui_update_lock_status(LOCK_STATUS_UNLOCKED);
        ui_show_countdown(10);
    }
}

ui_register_event_callback(my_callback, NULL);
```

### 场景 2：快速连续事件

```c
// 用户快速点击
ui_show_countdown(10);      // COUNTDOWN_END 入队
ui_cancel_countdown();      // COUNTDOWN_CANCEL 入队

// 主循环处理
ui_manager_process();       // 处理所有待执行动作
// 结果：两个事件都被处理，回调在锁外调用
```

### 场景 3：回调内部触发新事件

```c
void my_callback(ui_event_t event, void *data, void *user_data)
{
    if (event == UI_EVENT_COUNTDOWN_END) {
        // 在回调内部触发新事件（安全）
        ui_show_alarm(ALARM_ANTI_PINCH);
    }
}
```

## 注意事项

1. **回调执行时间**：回调在锁外执行，不会阻塞其他线程
2. **状态一致性**：界面切换在锁内执行，状态始终一致
3. **事件顺序**：保证 FIFO 顺序处理
4. **锁的获取释放**：确保每次 `ui_lock()` 都有对应的 `ui_unlock()`

## 版本历史

- **v2.3.1**：修复回调重入风险
- **v2.3.0**：使用环形缓冲区（有回调重入风险）
- **v2.2.0**：使用单个槽位（有事件丢失风险）
- **v2.1.0**：直接在定时器回调中切换界面（有跨锁风险）
