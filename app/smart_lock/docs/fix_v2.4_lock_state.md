# v2.4 锁状态管理修复

## 问题描述

1. `process_pending_action` 的 deinitializing 分支没有释放锁
2. `ui_manager_process()` 调用 `process_pending_action()` 后没有处理锁状态

## 修复方案

### 问题 1：deinitializing 分支必须先 unlock 再 return

**修复前**：
```c
if (g_ui_manager.deinitializing) {
    UI_LOGW("Deinitializing, skip callbacks");
    return count;  /* 直接返回，锁仍被持有 */
}
```

**修复后**：
```c
if (g_ui_manager.deinitializing) {
    UI_LOGW("Deinitializing, skip callbacks");
    ui_unlock();  /* 先释放锁 */
    return count; /* 再返回，此时锁未被持有 */
}
```

### 问题 2：ui_manager_process() 处理锁状态

**修复前**：
```c
int ui_manager_process(void)
{
    if (ui_trylock() != 0) return 0;
    int ret = process_pending_action();
    ui_unlock();  /* 总是释放锁 */
    return ret;
}
```

**修复后**：
```c
int ui_manager_process(void)
{
    if (ui_trylock() != 0) return 0;
    int ret = process_pending_action();

    /* 修复：检查 deinitializing，如果正在反初始化，不释放锁 */
    if (!g_ui_manager.deinitializing) {
        ui_unlock();
    }
    /* 如果 deinitializing，process_pending_action 已经释放锁，不再释放 */

    return ret;
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
     - deinitializing = false
     - ui_unlock()    → 锁计数 = 0
     - 调用回调       → 锁计数 = 0
     - ui_lock()      → 锁计数 = 1
     - return         → 锁计数 = 1
  3. deinitializing = false
  4. ui_unlock()      → 锁计数 = 0
```

### 场景 2：deinitializing 在处理前

```
ui_manager_process():
  1. ui_trylock()     → 锁计数 = 1
  2. process_pending_action():
     - 复制动作       → 锁计数 = 1
     - 界面切换       → 锁计数 = 1
     - deinitializing = true
     - ui_unlock()    → 锁计数 = 0
     - return         → 锁计数 = 0
  3. deinitializing = true
  4. 不调用 ui_unlock()（因为 process_pending_action 已释放）
```

### 场景 3：deinitializing 在回调期间

```
线程 A（ui_manager_process）     线程 B（ui_manager_deinit）
─────────────────────────────    ─────────────────────────────
ui_trylock() → 成功              
process_pending_action():        
  复制动作                       
  界面切换                       
  deinitializing = false         
  ui_unlock()                    
  调用回调                       
                                 deinitializing = true
                                 ui_lock() → 成功
                                 释放资源...
                                 ui_unlock()
                                 pthread_mutex_destroy()
  ui_lock() → 成功（递归锁）     
  return                         
deinitializing = true            
不调用 ui_unlock()               
return                           
```

---

## 锁管理约定

### process_pending_action()

| 情况 | 进入时 | 返回时 |
|------|--------|--------|
| 正常处理 | 锁被持有 | 锁被持有 |
| deinitializing | 锁被持有 | 锁未被持有 |

### ui_manager_process()

| 情况 | 调用前 | 调用后 |
|------|--------|--------|
| 正常处理 | 获取锁 | 释放锁 |
| deinitializing | 获取锁 | 不释放锁（process_pending_action 已释放） |

---

## 测试建议

### 1. 正常流程测试
```c
// 测试 ui_manager_process 正常工作
ui_manager_init();
ui_show_countdown(10);
ui_manager_process();  // 应该处理倒计时结束事件
ui_manager_deinit();
```

### 2. deinitializing 测试
```c
// 测试反初始化时的处理
ui_manager_init();
ui_show_countdown(10);

// 在另一个线程中反初始化
pthread_create(&thread, NULL, deinit_thread, NULL);

ui_manager_process();  // 应该安全处理
```

### 3. 并发测试
```c
// 测试并发安全性
for (int i = 0; i < 1000; i++) {
    ui_manager_init();
    ui_show_countdown(10);
    ui_manager_process();
    ui_manager_deinit();
}
```

---

## 版本历史

- **v2.4.2**：修复锁状态管理
- **v2.4.1**：修复 process_pending_action 竞态条件
- **v2.4.0**：架构统一，移除 notify_ui_event
