# v2.4 最终修复

## 修复内容

### 问题 1：process_pending_action 引用计数逻辑错误

**问题描述**：
原代码中 `process_pending_action()` 的锁契约复杂且容易出错：
- 如果进入 deinitializing 分支，直接 `ui_unlock(); return count;`
- 引用计数逻辑与锁管理混在一起
- 调用者难以理解锁状态

**修复方案**：
将 `process_pending_action()` 拆分为：
- `copy_pending_actions()`：只负责复制动作（在锁内）
- `ui_manager_process()`：负责所有锁管理和回调执行

```c
/* 新设计 */
int ui_manager_process(void)
{
    /* 1. trylock 获取锁 */
    /* 2. 复制动作（在锁内） */
    /* 3. 执行界面切换（在锁内） */
    /* 4. callback_refcount++ */
    /* 5. 释放锁 */
    /* 6. 执行回调（在锁外） */
    /* 7. 重新获取锁 */
    /* 8. callback_refcount--，如果归零则 signal */
    /* 9. 释放锁 */
}
```

---

### 问题 2：ui_manager_deinit() 忙等待可移植性问题

**问题描述**：
原代码使用 `usleep(1000)` 忙等待引用计数归零：
- 信号可以打断 `usleep`，提前返回
- 嵌入式 RTOS 中不推荐忙等待
- 浪费 CPU 资源

**修复方案**：
使用 `pthread_cond_t` 条件变量：

```c
/* 管理器结构体 */
typedef struct {
    // ...
    pthread_cond_t callback_done;  /* 条件变量：回调执行完成 */
    // ...
} ui_manager_t;

/* 初始化 */
pthread_cond_init(&g_ui_manager.callback_done, NULL);

/* ui_manager_process() 中，引用计数归零时发出信号 */
if (g_ui_manager.callback_refcount == 0) {
    pthread_cond_signal(&g_ui_manager.callback_done);
}

/* ui_manager_deinit() 中，等待引用计数归零 */
while (g_ui_manager.callback_refcount > 0) {
    pthread_cond_wait(&g_ui_manager.callback_done, &g_ui_manager.mutex);
}

/* 销毁 */
pthread_cond_destroy(&g_ui_manager.callback_done);
```

---

### 问题 3：ui_manager_process() 锁契约脆弱

**问题描述**：
原代码中 `process_pending_action()` 的锁契约复杂：
- 进入时锁被持有
- 返回时可能锁被持有，也可能未被持有
- 调用者需要检查 deinitializing 来决定是否释放锁

**修复方案**：
简化 `ui_manager_process()` 的锁契约：
- 进入时：通过 trylock 获取锁
- 返回时：锁被释放

```c
int ui_manager_process(void)
{
    /* 1. trylock 获取锁 */
    if (ui_trylock() != 0) return 0;

    /* 2. 复制动作、执行界面切换（在锁内） */
    // ...

    /* 3. callback_refcount++ */
    g_ui_manager.callback_refcount++;

    /* 4. 释放锁 */
    ui_unlock();

    /* 5. 执行回调（在锁外） */
    // ...

    /* 6. 重新获取锁 */
    ui_lock();

    /* 7. callback_refcount--，如果归零则 signal */
    g_ui_manager.callback_refcount--;
    if (g_ui_manager.callback_refcount == 0) {
        pthread_cond_signal(&g_ui_manager.callback_done);
    }

    /* 8. 释放锁 */
    ui_unlock();

    return count;
}
```

---

## 锁契约总结

### ui_manager_process()

| 阶段 | 锁状态 | 说明 |
|------|--------|------|
| 进入 | 未持有 | 通过 trylock 获取 |
| 复制动作 | 持有 | 在锁内操作队列 |
| 界面切换 | 持有 | 在锁内操作 LVGL |
| callback_refcount++ | 持有 | 保护 mutex 生命周期 |
| 释放锁 | 未持有 | 准备执行回调 |
| 执行回调 | 未持有 | 在锁外调用业务回调 |
| 重新获取锁 | 持有 | 准备更新引用计数 |
| callback_refcount-- | 持有 | 减少引用计数 |
| signal | 持有 | 通知 deinit（如果归零） |
| 释放锁 | 未持有 | 函数返回 |

### ui_manager_deinit()

| 阶段 | 锁状态 | 说明 |
|------|--------|------|
| 进入 | 未持有 | - |
| 设置 deinitializing | 未持有 | 阻止新的回调 |
| 获取锁 | 持有 | 准备销毁资源 |
| 销毁资源 | 持有 | 删除定时器、销毁界面 |
| 等待 refcount == 0 | 持有 → 等待 | pthread_cond_wait 释放锁并等待 |
| 收到信号 | 持有 | pthread_cond_wait 重新获取锁 |
| 释放锁 | 未持有 | - |
| 销毁 cond | 未持有 | - |
| 销毁 mutex | 未持有 | - |

---

## 条件变量工作原理

### pthread_cond_wait() 行为

```
调用前：锁被持有
调用时：
  1. 释放锁
  2. 等待信号
  3. 收到信号后重新获取锁
调用后：锁被持有
```

### 信号发送时机

```c
/* ui_manager_process() 中 */
g_ui_manager.callback_refcount--;
if (g_ui_manager.callback_refcount == 0) {
    pthread_cond_signal(&g_ui_manager.callback_done);  /* 通知 deinit */
}
```

### 等待逻辑

```c
/* ui_manager_deinit() 中 */
while (g_ui_manager.callback_refcount > 0) {
    /* 如果 refcount > 0，释放锁并等待 */
    /* 收到信号后重新获取锁，再次检查条件 */
    pthread_cond_wait(&g_ui_manager.callback_done, &g_ui_manager.mutex);
}
/* refcount == 0，继续执行 */
```

---

## 安全性保证

### mutex 销毁前提条件

`pthread_mutex_destroy()` 要求：
- 没有线程持有 mutex
- 没有线程等待 mutex

**保证**：
1. `ui_manager_deinit()` 设置 `deinitializing = true`
2. `ui_manager_process()` 检查 `deinitializing`，如果为 true 则不执行回调
3. `ui_manager_deinit()` 等待 `callback_refcount == 0`
4. `callback_refcount == 0` 意味着没有线程正在执行回调
5. 此时没有线程持有或等待 mutex，可以安全销毁

### 时序分析

```
T1: ui_manager_process()        T2: ui_manager_deinit()
    ├─ trylock ✓                    
    ├─ 复制动作                     
    ├─ 界面切换                     
    ├─ callback_refcount++ (1)      
    ├─ ui_unlock()                  
    ├─ 执行回调...                  ├─ deinitializing = true
    │                               ├─ ui_lock() ✓
    │                               ├─ 销毁资源
    │                               ├─ while (refcount > 0)
    │                               │   └─ pthread_cond_wait()
    │                               │       ├─ ui_unlock()
    │                               │       └─ 等待信号...
    ├─ ui_lock() ✓                  
    ├─ callback_refcount-- (0)      
    ├─ pthread_cond_signal() ──────▶├─ 收到信号
    │                               │   └─ ui_lock() ✓
    ├─ ui_unlock()                  ├─ refcount == 0
    └─ return                       ├─ ui_unlock()
                                    ├─ pthread_cond_destroy()
                                    ├─ pthread_mutex_destroy() ✓
                                    └─ return
```

---

## ui_trylock() 非阻塞特性说明

### 设计取舍

```c
if (ui_trylock() != 0) return 0;  /* 非阻塞 */
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
    ├─ 处理所有积压事件              
    └─ return N
```

---

## 版本历史

- **v2.4.4**：简化锁契约，使用条件变量
- **v2.4.3**：修复并发时序漏洞（引用计数保护）
- **v2.4.2**：修复锁状态管理
- **v2.4.1**：修复 process_pending_action 竞态条件
- **v2.4.0**：架构统一，移除 notify_ui_event
