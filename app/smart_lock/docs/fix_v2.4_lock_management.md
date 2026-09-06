# v2.4 锁管理修复

## 锁管理约定

### process_pending_action()

**进入时**：锁被持有
**返回时**：锁被持有（无论是否执行回调）

```c
static int process_pending_action(void)
{
    /* 1. 复制动作（锁内） */
    // ...

    /* 2. 执行界面切换（锁内） */
    // ...

    /* 3. 检查 deinitializing */
    if (g_ui_manager.deinitializing) {
        return count;  /* 直接返回，锁仍被持有 */
    }

    /* 4. 释放锁，调用回调 */
    ui_unlock();
    // ... 调用回调 ...

    /* 5. 重新获取锁 */
    ui_lock();

    return count;  /* 返回时锁被持有 */
}
```

### ui_manager_process()

```c
int ui_manager_process(void)
{
    if (ui_trylock() != 0) return 0;  /* 获取锁 */

    int ret = process_pending_action(); /* 内部可能 unlock/lock，但返回时锁被持有 */

    ui_unlock();  /* 释放锁 */
    return ret;
}
```

### LVGL 回调

```c
static void btn_event_cb(lv_event_t *e)
{
    if (ui_trylock() != 0) return;  /* 获取锁 */

    set_pending_action(...);  /* 添加到队列 */

    ui_unlock();  /* 释放锁 */
}
```

---

## 锁状态分析

### 场景 1：正常处理

```
ui_manager_process():
  1. ui_trylock()     → 锁计数 = 1
  2. process_pending_action():
     - 复制动作       → 锁计数 = 1
     - 界面切换       → 锁计数 = 1
     - ui_unlock()    → 锁计数 = 0
     - 调用回调       → 锁计数 = 0
     - ui_lock()      → 锁计数 = 1
     - return         → 锁计数 = 1
  3. ui_unlock()      → 锁计数 = 0
```

### 场景 2：deinitializing

```
ui_manager_process():
  1. ui_trylock()     → 锁计数 = 1
  2. process_pending_action():
     - 复制动作       → 锁计数 = 1
     - 界面切换       → 锁计数 = 1
     - 检查 deinitializing = true
     - return         → 锁计数 = 1（不释放锁）
  3. ui_unlock()      → 锁计数 = 0
```

### 场景 3：ui_manager_deinit() 并发

```
线程 A（ui_manager_process）     线程 B（ui_manager_deinit）
─────────────────────────────    ─────────────────────────────
ui_trylock() → 成功              
process_pending_action():        
  复制动作                       
  界面切换                       
  检查 deinitializing = false    
                                 g_ui_manager.deinitializing = true
                                 ui_lock() → 等待...
  ui_unlock()                    
  调用回调                       
                                 ui_lock() → 成功
  ui_lock() → 等待...            
                                 释放资源...
                                 ui_unlock()
                                 pthread_mutex_destroy()
                                 
⚠️ 问题：ui_lock() 尝试获取已销毁的 mutex
```

**解决方案**：在释放锁之前再次检查 deinitializing

```c
static int process_pending_action(void)
{
    // ... 复制动作，界面切换 ...

    /* 检查 deinitializing */
    if (g_ui_manager.deinitializing) {
        return count;  /* 不释放锁，直接返回 */
    }

    ui_unlock();

    // ... 调用回调 ...

    /* 重新获取锁前再次检查 */
    if (g_ui_manager.deinitializing) {
        return count;  /* 不重新获取锁，直接返回 */
    }

    ui_lock();
    return count;
}
```

---

## 最终修复方案

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

    /* 3. 检查是否正在反初始化 */
    if (g_ui_manager.deinitializing) {
        UI_LOGW("Deinitializing, skip callbacks");
        return count;  /* 直接返回，锁仍被持有 */
    }

    /* 4. 释放锁后调用回调 */
    ui_event_cb_t cb = g_ui_manager.event_callback;
    void *user_data = g_ui_manager.event_user_data;
    ui_unlock();

    for (int i = 0; i < count; i++) {
        if (actions[i].event != UI_EVENT_NONE && cb) {
            UI_LOGD("Notifying event: %d", actions[i].event);
            cb(actions[i].event, actions[i].data, user_data);
        }
    }

    /* 5. 重新获取锁前再次检查 deinitializing */
    if (g_ui_manager.deinitializing) {
        UI_LOGW("Deinitializing during callback, skip re-lock");
        return count;  /* 不重新获取锁，调用者需要处理 */
    }

    ui_lock();
    return count;
}
```

**但是**，这个方案有问题：如果在步骤5 检查到 deinitializing，不重新获取锁就返回，那么 `ui_manager_process()` 会尝试 unlock，但锁未被持有，这是错误。

**正确方案**：让 `ui_manager_process()` 处理这种情况：

```c
int ui_manager_process(void)
{
    if (!g_ui_manager.initialized) return -1;
    if (ui_trylock() != 0) return 0;

    /* process_pending_action 内部可能 unlock/lock */
    /* 如果 deinitializing 发生在回调期间，process_pending_action 不会重新获取锁 */
    bool lock_held = true;
    int ret = process_pending_action(&lock_held);

    /* 只有锁仍被持有时才释放 */
    if (lock_held) {
        ui_unlock();
    }

    return ret;
}
```

但这增加了复杂性。**最简单的方案**是：不释放锁调用回调，使用异步方式通知事件。

---

## 推荐方案

保持当前实现，但添加保护：

```c
static int process_pending_action(void)
{
    // ... 复制动作，界面切换 ...

    /* 检查 deinitializing */
    if (g_ui_manager.deinitializing) {
        return count;  /* 直接返回，锁仍被持有 */
    }

    ui_unlock();

    // ... 调用回调 ...

    ui_lock();  /* 假设 deinitializing 不会在回调期间发生 */
    return count;
}
```

**假设**：业务回调执行时间很短，`ui_manager_deinit()` 不会在回调执行期间被调用。

**如果需要严格保护**，可以使用标志位：

```c
static bool in_callback = false;

static int process_pending_action(void)
{
    // ...

    ui_unlock();
    in_callback = true;

    // ... 调用回调 ...

    in_callback = false;

    if (g_ui_manager.deinitializing) {
        /* 回调期间发生 deinit，不重新获取锁 */
        /* ui_manager_process 需要检查 in_callback */
        return count;
    }

    ui_lock();
    return count;
}
```

---

## 总结

当前实现的锁管理：

1. **process_pending_action()**：进入时锁被持有，返回时锁被持有
2. **ui_manager_process()**：获取锁，调用 process_pending_action，释放锁
3. **deinitializing 保护**：检查标记，避免在 mutex 销毁后操作

**风险**：如果 `ui_manager_deinit()` 在回调执行期间被调用，可能导致竞态条件。

**缓解**：业务回调应该快速返回，避免长时间持有锁。
