# v2.4 严重问题修复

## 问题 1：process_pending_action 竞态条件（崩溃风险）

### 问题描述

`process_pending_action()` 中存在严重的竞态条件：

```c
static int process_pending_action(void)
{
    // ... 复制动作到临时数组 ...

    /* 执行界面切换（在锁内） */

    /* 释放锁后调用回调 */
    ui_event_cb_t cb = g_ui_manager.event_callback;  // 读取副本
    void *user_data = g_ui_manager.event_user_data;
    ui_unlock();                                      // 释放锁

    for (int i = 0; i < count; i++) {
        if (actions[i].event != UI_EVENT_NONE && cb) {
            cb(actions[i].event, actions[i].data, user_data);  // 调用回调
        }
    }

    ui_lock();  // ⚠️ 危险：mutex 可能已被销毁！
    return count;
}
```

**竞态场景**：
1. `process_pending_action()` 读取 cb 副本，释放锁
2. 回调开始执行
3. 其他线程调用 `ui_manager_deinit()`
4. `ui_manager_deinit()` 销毁 mutex
5. 回调执行完成
6. `process_pending_action()` 尝试重新获取锁
7. **崩溃**：mutex 已被销毁，`pthread_mutex_lock()` 返回未定义行为

### 修复方案

在释放锁之前检查 `deinitializing` 标记，如果正在反初始化，不执行回调，不释放锁。

```c
static int process_pending_action(void)
{
    // ... 复制动作到临时数组 ...执行界面切换（在锁内） */

    ui_event_cb_t cb = g_ui_manager.event_callback;
    void *user_data = g_ui_manager.event_user_data;

    /* 修复：在释放锁之前检查 deinitializing 标记 */
    if (g_ui_manager.deinitializing) {
        UI_LOGW("Deinitializing, skip callbacks");
        return count;  // 不释放锁，不执行回调
    }

    ui_unlock();

    for (int i = 0; i < count; i++) {
        if (actions[i].event != UI_EVENT_NONE && cb) {
            cb(actions[i].event, actions[i].data, user_data);
        }
    }

    ui_lock();
    return count;
}
```

### 修复流程

```
修复前（有崩溃风险）：
1. 读取 cb 副本
2. 释放锁
3. 调用回调
4. 重新获取锁 ← 危险：mutex 可能已被销毁

修复后（安全）：
1. 读取 cb 副本
2. 检查 deinitializing
3. 如果正在反初始化，直接返回（不释放锁）
4. 如果未反初始化，释放锁
5. 调用回调
6. 重新获取锁
```

### 安全性分析

- **deinitializing 为 true**：说明其他线程正在执行 `ui_manager_deinit()`
  - 此时 mutex 还未被销毁（`ui_manager_deinit()` 需要先获取锁）
  - 我们不释放锁，直接返回
  - `ui_manager_deinit()` 会等待我们释放锁（但我们不释放，直到 `ui_manager_deinit()` 获取锁）
  - 实际上，由于我们使用的是递归锁，`ui_manager_deinit()` 可以获取锁
  - 但我们不释放锁，`ui_manager_deinit()` 会等待
  - 这会导致死锁！

  等等，让我重新分析：

  实际上，`ui_manager_deinit()` 的流程是：
  1. `g_ui_manager.deinitializing = true;`
  2. `ui_lock();`
  3. 释放资源...
  4. `ui_unlock();`
  5. `pthread_mutex_destroy(&g_ui_manager.mutex);`

  如果我们在 `process_pending_action()` 中检查到 `deinitializing == true`，说明：
  - 其他线程已经设置了 `deinitializing = true`
  - 但还没有获取锁（或者已经获取了锁）
  
  如果其他线程已经获取了锁，那么我们无法获取锁（trylock 会失败）
  如果其他线程还没有获取锁，那么我们持有锁，其他线程会等待

  最安全的方案是：检查 `deinitializing` 后，不释放锁，直接返回
  这样：
  - 如果其他线程正在执行 `ui_manager_deinit()`，它会等待我们释放锁
  - 但我们不释放锁，直到当前函数返回
  - 函数返回后，调用者会释放锁（在 `ui_manager_process()` 中）
  - 然后 `ui_manager_deinit()` 可以获取锁并继续执行
  - 最后销毁 mutex

  这个方案是安全的，因为：
  1. 我们不释放锁，避免了 mutex 被销毁后重新获取的风险
  2. 函数返回后，调用者会释放锁
  3. `ui_manager_deinit()` 可以正常获取锁并继续执行

---

## 问题 2：ui_show_alarm 架构不一致

### 问题描述

`ui_show_alarm()` 内部直接调用 `ui_switch_screen_internal()`，不走 pending 队列，违反架构原则。

```c
// 修复前（架构不一致）
int ui_show_alarm(alarm_type_t type)
{
    ui_lock();
    set_pending_action(UI_EVENT_NONE, NULL, UI_SCREEN_ALERT, true);  // 走队列
    
    // 但是立即操作控件，此时界面可能还未创建
    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_label)) {
        lv_label_set_text(...);  // ⚠️ 界面可能还未创建
    }
    
    ui_unlock();
}
```

### 修复方案

先创建界面，再设置控件，最后走 pending 队列切换界面。

```c
// 修复后（架构一致）
int ui_show_alarm(alarm_type_t type)
{
    ui_lock();

    /* 先创建报警界面（如果未创建） */
    if (g_ui_manager.screen_created[UI_SCREEN_ALERT] != SCREEN_CREATED) {
        if (create_alarm_screen() < 0) {
            ui_unlock();
            return -1;
        }
    }

    /* 设置报警内容 */
    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_label)) {
        lv_label_set_text(...);
    }

    /* 创建动画 */
    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_icon)) {
        lv_anim_start(...);
    }

    /* 统一走 pending 队列切换界面 */
    set_pending_action(UI_EVENT_NONE, NULL, UI_SCREEN_ALERT, true);

    ui_unlock();
    return 0;
}
```

### 修复流程

```
修复前（架构不一致）：
1. set_pending_action()
2. 立即操作控件（界面可能未创建）
3. ui_manager_process() 中创建界面

修复后（架构一致）：
1. 创建界面（如果未创建）
2. 设置控件
3. set_pending_action()
4. ui_manager_process() 中切换界面
```

### 优点

1. **控件操作在界面创建之后**：确保控件存在
2. **界面切换走 pending 队列**：架构一致
3. **避免竞态条件**：所有操作在锁内完成

---

## 测试建议

### 1. 崩溃测试
```c
// 测试反初始化时的回调执行
void test_deinit_during_callback(void)
{
    ui_manager_init();
    ui_register_event_callback(my_callback, NULL);

    // 触发事件
    ui_show_countdown(10);

    // 在回调执行期间反初始化
    // （需要多线程测试）
    ui_manager_deinit();
}
```

### 2. 架构一致性测试
```c
// 测试 ui_show_alarm 走 pending 队列
void test_show_alarm_pending(void)
{
    ui_manager_init();

    ui_show_alarm(ALARM_ANTI_PINCH);

    // 检查是否走 pending 队列
    assert(ui_get_current_screen() == UI_SCREEN_MAIN);  // 还未切换

    ui_manager_process();
    assert(ui_get_current_screen() == UI_SCREEN_ALERT);  // 现在切换了
}
```

---

## 版本历史

- **v2.4.1**：修复 process_pending_action 竞态条件，修复 ui_show_alarm 架构不一致
- **v2.4.0**：架构统一，移除 notify_ui_event
- **v2.3.1**：修复回调重入风险（仍有架构问题）
- **v2.3.0**：使用环形缓冲区（有回调重入风险）
- **v2.2.0**：使用单个槽位（有事件丢失风险）
- **v2.1.0**：直接在定时器回调中切换界面（有跨锁风险）
