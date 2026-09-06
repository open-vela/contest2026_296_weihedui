# v2.4 并发时序漏洞修复

## 问题描述

`ui_manager_deinit()` 和 `ui_manager_process()` 存在严重的并发时序漏洞：

```
时间线：
T1: ui_manager_process()        T2: ui_manager_deinit()
    ├─ ui_trylock() ✓               
    ├─ process_pending_action()      
    │   ├─ 拷贝 actions, cb          
    │   ├─ deinitializing=false      
    │   ├─ ui_unlock()               
    │   ├─ cb() 执行中...            ├─ ui_lock() ✓
    │   │                            ├─ deinitializing=true
    │   │                            ├─ 销毁所有对象
    │   │                            ├─ pthread_mutex_destroy() ← mutex 被销毁
    │   │                            └─ ui_unlock()
    │   └─ ui_lock() ← CRASH! 未定义行为
    └─ ui_unlock()
```

**根因**：`pthread_mutex_destroy()` 要求没有线程持有或等待该 mutex，但场景违反了这个前提。

## 修复方案

使用**引用计数**保护 mutex 生命周期：

```c
typedef struct {
    // ...
    volatile int callback_refcount;  /* 回调执行引用计数 */
    // ...
} ui_manager_t;
```

### process_pending_action()

```c
static int process_pending_action(void)
{
    // ... 复制动作，界面切换 ...

    /* 检查 deinitializing */
    if (g_ui_manager.deinitializing) {
        ui_unlock();
        return count;
    }

    /* 增加引用计数，保护 mutex 生命周期 */
    g_ui_manager.callback_refcount++;

    /* 释放锁后调用回调 */
    ui_event_cb_t cb = g_ui_manager.event_callback;
    void *user_data = g_ui_manager.event_user_data;
    ui_unlock();

    for (int i = 0; i < count; i++) {
        if (actions[i].event != UI_EVENT_NONE && cb) {
            cb(actions[i].event, actions[i].data, user_data);
        }
    }

    /* 重新获取锁 */
    ui_lock();

    /* 减少引用计数 */
    g_ui_manager.callback_refcount--;

    return count;
}
```

### ui_manager_deinit()

```c
int ui_manager_deinit(void)
{
    g_ui_manager.deinitializing = true;

    ui_lock();
    // ... 销毁资源 ...
    ui_unlock();

    /* 等待所有回调执行完成（引用计数归零） */
    while (g_ui_manager.callback_refcount > 0) {
        UI_LOGW("Waiting for callbacks to complete (refcount=%d)",
                g_ui_manager.callback_refcount);
        usleep(1000);  /* 1ms */
    }

    /* 此时没有线程持有或等待 mutex，可以安全销毁 */
    pthread_mutex_destroy(&g_ui_manager.mutex);

    return 0;
}
```

## 修复后的时序

```
时间线：
T1: ui_manager_process()        T2: ui_manager_deinit()
    ├─ ui_trylock() ✓               
    ├─ process_pending_action()      
    │   ├─ 拷贝 actions, cb          
    │   ├─ deinitializing=false      
    │   ├─ callback_refcount++ (1)   
    │   ├─ ui_unlock()               
    │   ├─ cb() 执行中...            ├─ ui_lock() ✓
    │   │                            ├─ deinitializing=true
    │   │                            ├─ 销毁资源
    │   │                            ├─ ui_unlock()
    │   │                            ├─ while (callback_refcount > 0)
    │   │                            │   └─ usleep(1ms) ← 等待回调完成
    │   ├─ ui_lock() ✓               
    │   ├─ callback_refcount-- (0)   
    │   └─ return                    
    └─ ui_unlock()                   ├─ callback_refcount == 0
                                     ├─ pthread_mutex_destroy() ✓ 安全
                                     └─ return
```

## 引用计数保护机制

### 原子性

```c
volatile int callback_refcount;
```

- 使用 `volatile` 确保多线程可见性
- 增减操作在锁内执行（`process_pending_action` 持有锁时）
- 读取操作在锁外执行（`ui_manager_deinit` 轮询）

### 状态转换

```
callback_refcount 变化：

初始状态：0（无回调执行）

process_pending_action():
  1. ui_lock()
  2. callback_refcount++ → 1（回调即将执行）
  3. ui_unlock()
  4. 执行回调
  5. ui_lock()
  6. callback_refcount-- → 0（回调执行完成）
  7. ui_unlock()

ui_manager_deinit():
  1. 设置 deinitializing = true
  2. 释放锁
  3. while (callback_refcount > 0) usleep(1ms)
  4. callback_refcount == 0 时继续
  5. pthread_mutex_destroy()
```

## ui_trylock() 非阻塞特性说明

### 设计取舍

```c
int ui_manager_process(void)
{
    if (ui_trylock() != 0) return 0;  /* 非阻塞 */
    // ...
}
```

**优点**：
- 不阻塞主循环
- 保持系统响应性

**缺点**：
- 如果锁被占用，队列不消费
- 事件会积压在 pending 队列

**适用场景**：
- 主循环需要保持高响应性
- 事件处理可以容忍少量延迟

### 积压处理

```
时间线：
T1: ui_manager_process()        T2: 其他线程持有锁
    ├─ ui_trylock() ← 失败          
    └─ return 0（队列不消费）        

T3: ui_manager_process()        T4: 其他线程释放锁
    ├─ ui_trylock() ✓               
    ├─ process_pending_action()      
    │   └─ 处理所有积压事件          
    └─ return N（处理了N个事件）
```

**文档说明**：
- `ui_manager_process()` 使用非阻塞锁
- 如果锁被占用，返回 0，队列不消费
- 事件会积压在 pending 队列
- 下次调用时会批量处理所有积压事件
- 这是设计取舍，避免阻塞主循环

## 测试建议

### 1. 并发测试

```c
void* deinit_thread(void* arg)
{
    usleep(100);  /* 等待回调开始执行 */
    ui_manager_deinit();
    return NULL;
}

void test_concurrent_deinit(void)
{
    ui_manager_init();
    ui_register_event_callback(slow_callback, NULL);
    ui_show_countdown(10);

    pthread_t thread;
    pthread_create(&thread, NULL, deinit_thread, NULL);

    ui_manager_process();  /* 应该安全完成 */
    pthread_join(thread, NULL);
}
```

### 2. 引用计数测试

```c
void test_refcount(void)
{
    ui_manager_init();

    /* 模拟回调执行 */
    ui_lock();
    g_ui_manager.callback_refcount = 1;
    ui_unlock();

    /* deinit 应该等待 */
    pthread_t thread;
    pthread_create(&thread, NULL, deinit_thread, NULL);

    usleep(10000);  /* 10ms */
    g_ui_manager.callback_refcount = 0;  /* 模拟回调完成 */

    pthread_join(thread, NULL);
}
```

### 3. 积压处理测试

```c
void test_backlog(void)
{
    ui_manager_init();

    /* 快速添加多个事件 */
    for (int i = 0; i < 100; i++) {
        ui_show_countdown(10);
    }

    /* 模拟锁被占用 */
    ui_lock();
    usleep(100000);  /* 100ms */

    /* 释放锁 */
    ui_unlock();

    /* 处理所有积压事件 */
    int processed = ui_manager_process();
    assert(processed == 100);
}
```

---

## 版本历史

- **v2.4.3**：修复并发时序漏洞（引用计数保护）
- **v2.4.2**：修复锁状态管理
- **v2.4.1**：修复 process_pending_action 竞态条件
- **v2.4.0**：架构统一，移除 notify_ui_event
