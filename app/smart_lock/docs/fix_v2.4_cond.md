# v2.4 条件变量和锁获取修复

## 修复内容

### 问题 1：pthread_cond_signal 应该在锁外调用

**问题描述**：
原代码在持有锁时调用 `pthread_cond_signal()`：
```c
ui_lock();
callback_refcount--;
if (callback_refcount == 0) {
    pthread_cond_signal(&callback_done);  // 在锁内调用
}
ui_unlock();
```

这可能导致"惊群效应"：被唤醒的线程立即尝试获取锁，但锁仍被当前线程持有。

**修复方案**：
在锁外调用 `pthread_cond_signal()`：
```c
ui_lock();
callback_refcount--;
bool should_signal = (callback_refcount == 0);
ui_unlock();

// 在锁外发送信号
if (should_signal) {
    pthread_cond_signal(&callback_done);
}
```

**优点**：
- 避免惊群效应
- 被唤醒的线程可以立即获取锁
- 提高并发性能

---

### 问题 2：ui_trylock() 非阻塞特性

**问题描述**：
原代码使用 `ui_trylock()` 一次，失败直接返回 0：
```c
if (ui_trylock() != 0) {
    return 0;  // 锁被占用，队列不消费
}
```

如果锁被短暂占用（如 LVGL 回调处理），事件会积压在队列。

**修复方案**：
添加重试机制，最多重试 3 次：
```c
int retry = 3;
while (ui_trylock() != 0) {
    if (--retry <= 0) {
        return 0;  // 3 次都失败，队列不消费
    }
    usleep(100);  // 100us，短暂让出 CPU
}
```

**设计取舍**：
- **重试 3 次**：平衡响应性和事件处理及时性
- **100us 间隔**：短暂让出 CPU，不阻塞主循环
- **最终失败**：返回 0，事件积压在队列下次处理

---

## 锁管理约定（v2.4.5 最终版）

### ui_manager_process()

```
进入时：通过 trylock 获取锁（最多重试 3 次）
返回时：锁被释放

内部流程：
1. trylock 获取锁（最多重试 3 次，每次间隔 100us）
2. 复制动作、执行界面切换（在锁内）
3. callback_refcount++（保护 mutex 生命周期）
4. 释放锁
5. 执行回调（在锁外）
6. 重新获取锁
7. callback_refcount--，保存 should_signal
8. 释放锁
9. 如果 should_signal，在锁外 pthread_cond_signal
```

### ui_manager_deinit()

```
进入时：未持有锁
返回时：未持有锁

内部流程：
1. 设置 deinitializing = true
2. 获取锁
3. 销毁资源
4. while (callback_refcount > 0)
   - pthread_cond_wait 等待信号
5. 释放锁
6. pthread_cond_destroy
7. pthread_mutex_destroy
```

---

## pthread_cond_signal 最佳实践

### 在锁内调用（不推荐）

```c
pthread_mutex_lock(&mutex);
// ... 操作共享数据 ...
pthread_cond_signal(&cond);  // 唤醒等待线程
pthread_mutex_unlock(&mutex);
// 被唤醒的线程尝试获取锁，但锁仍被持有，导致上下文切换
```

### 在锁外调用（推荐）

```c
pthread_mutex_lock(&mutex);
// ... 操作共享数据 ...
bool should_signal = (condition);
pthread_mutex_unlock(&mutex);

if (should_signal) {
    pthread_cond_signal(&cond);  // 唤醒等待线程
}
// 被唤醒的线程可以立即获取锁
```

### 为什么在锁外调用更好？

1. **避免惊群效应**：被唤醒的线程立即尝试获取锁，但锁仍被持有
2. **减少上下文切换**：被唤醒的线程可以立即获取锁
3. **提高并发性能**：等待线程和当前线程可以并行执行

---

## ui_trylock() 重试机制

### 设计考虑

| 参数 | 值 | 说明 |
|------|-----|------|
| 重试次数 | 3 | 平衡响应性和及时性 |
| 重试间隔 | 100us | 短暂让出 CPU |
| 最终失败 | 返回 0 | 事件积压在队列 |

### 时序分析

```
T1: ui_manager_process()
    ├─ ui_trylock() ← 失败
    ├─ usleep(100us)
    ├─ ui_trylock() ← 失败
    ├─ usleep(100us)
    ├─ ui_trylock() ← 成功
    └─ 处理事件...

T2: 其他线程
    ├─ 持有锁（100us）
    └─ 释放锁
```

### 积压处理

```
时间线：
T1: ui_manager_process()        T2: 其他线程持有锁
    ├─ ui_trylock() ← 失败          
    ├─ usleep(100us)                
    ├─ ui_trylock() ← 失败          
    ├─ usleep(100us)                
    ├─ ui_trylock() ← 失败          
    └─ return 0（队列不消费）        

T3: ui_manager_process()        T4: 其他线程释放锁
    ├─ ui_trylock() ✓               
    ├─ 处理所有积压事件              
    └─ return N
```

---

## 安全性保证

### pthread_cond_wait 前置条件

`pthread_cond_wait()` 要求调用时 mutex 被当前线程持有。

**保证**：
- `ui_manager_deinit()` 在调用 `pthread_cond_wait()` 前调用 `ui_lock()`
- `pthread_cond_wait()` 内部会释放锁并等待信号
- 收到信号后重新获取锁，然后返回

### pthread_cond_signal 时机

`pthread_cond_signal()` 可以在持有或不持有锁的情况下调用。

**最佳实践**：
- 在锁外调用，避免惊群效应
- 先保存条件，释放锁后再发送信号

### mutex 销毁前提条件

`pthread_mutex_destroy()` 要求：
- 没有线程持有 mutex
- 没有线程等待 mutex

**保证**：
- `callback_refcount == 0` 确保没有回调正在执行
- `pthread_cond_wait()` 返回后锁被持有，然后释放
- 此时没有线程持有或等待 mutex，可以安全销毁

---

## 版本历史

- **v2.4.5**：修复 pthread_cond_signal 调用时机，添加 trylock 重试机制
- **v2.4.4**：简化锁契约，使用条件变量
- **v2.4.3**：修复并发时序漏洞（引用计数保护）
- **v2.4.2**：修复锁状态管理
- **v2.4.1**：修复 process_pending_action 竞态条件
- **v2.4.0**：架构统一，移除 notify_ui_event
