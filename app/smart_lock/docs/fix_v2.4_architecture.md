# v2.4 架构统一修复

## 修复内容

### 问题 1：notify_ui_event 锁释放-重获取设计缺陷

**问题描述**
`notify_ui_event()` 释放锁再重新获取锁的设计存在重大缺陷：
- 释放锁期间，其他线程可能修改状态
- 重新获取锁后，状态可能已不一致
- 如果回调内部获取锁，可能导致死锁

**修复方案**
完全移除 `notify_ui_event()` 函数，所有事件统一走 pending 队列。

```c
// 修复前（有死锁风险）
static void notify_ui_event(ui_event_t event, void *data)
{
    ui_event_cb_t cb = g_ui_manager.event_callback;
    ui_unlock();          // ⚠️ 释放锁
    if (cb) cb(...);      // ⚠️ 在锁外调用
    ui_lock();            // ⚠️ 重新获取锁
}

// 修复后（安全）
// 删除 notify_ui_event()，所有事件走 pending 队列
static void btn_event_cb(lv_event_t *e)
{
    ui_trylock();
    set_pending_action(UI_EVENT_UNLOCK, NULL, UI_SCREEN_MAX, false);
    ui_unlock();
}

int ui_manager_process(void)
{
    ui_trylock();
    process_pending_action();  // 统一处理
    ui_unlock();
}
```

---

### 问题 2：部分 LVGL 回调直接执行业务回调

**问题描述**
原代码中部分回调直接调用业务回调，不走 pending 队列，导致：
- 架构不一致
- 难以维护
- 可能有重入风险

**修复方案**
所有 LVGL 回调统一使用 `set_pending_action()` 添加到队列。

```c
// 修复前（不一致）
static void unlock_btn_event_cb(lv_event_t *e)
{
    ui_lock();
    notify_ui_event(UI_EVENT_UNLOCK, NULL);  // 直接调用
    ui_unlock();
}

// 修复后（统一）
static void btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    ui_trylock();

    if (btn == g_ui_manager.main_screen.unlock_btn) {
        set_pending_action(UI_EVENT_UNLOCK, NULL, UI_SCREEN_MAX, false);
    } else if (btn == g_ui_manager.main_screen.lock_btn) {
        set_pending_action(UI_EVENT_LOCK, NULL, UI_SCREEN_MAX, false);
    }
    // ... 其他按钮

    ui_unlock();
}
```

---

### 问题 3：按钮回调直接调用 ui_switch_screen_internal()

**问题描述**
部分按钮回调在锁内直接调用 `ui_switch_screen_internal()`，违反架构原则。

**修复方案**
所有界面切换统一走 pending 队列。

```c
// 修复前（违反架构）
static void settings_btn_event_cb(lv_event_t *e)
{
    ui_lock();
    ui_switch_screen_internal(UI_SCREEN_SETTINGS);  // ⚠️ 直接调用
    ui_unlock();
}

// 修复后（统一）
static void btn_event_cb(lv_event_t *e)
{
    ui_trylock();
    set_pending_action(UI_EVENT_NONE, NULL, UI_SCREEN_SETTINGS, true);  // ✅ 走队列
    ui_unlock();
}
```

---

### 问题 4：strftime() 没有校验返回值

**问题描述**
`strftime()` 可能失败（返回 0），但原代码没有检查。

**修复方案**
添加返回值检查。

```c
// 修复前（无检查）
char buf[6];
strftime(buf, sizeof(buf), "%H:%M", tm_info);

// 修复后（有检查）
char buf[6];
size_t ret = strftime(buf, sizeof(buf), "%H:%M", tm_info);
if (ret == 0) {
    UI_LOGW("strftime failed or buffer too small");
    ui_unlock();
    return;
}
```

---

## 架构设计

### 统一事件处理流程

```
LVGL 回调                pending 队列              主循环
    │                        │                       │
    ▼                        ▼                       ▼
┌─────────┐            ┌─────────┐            ┌─────────┐
│ trylock │            │  入队   │            │  处理   │
└────┬────┘            └────┬────┘            └────┬────┘
     │                      │                      │
     ▼                      ▼                      ▼
┌─────────────┐       ┌─────────┐            ┌─────────────┐
│ set_pending │──────▶│  队列   │            │  切换界面   │
│ _action()   │       │         │            │  调用回调   │
└──────┬──────┘       └─────────┘            └──────┬──────┘
       │                                            │
       ▼                                            ▼
  ┌─────────┐                                  ┌─────────┐
  │ unlock  │                                  │ unlock  │
  └─────────┘                                  └─────────┘
```

### 回调函数统一

| 回调函数 | 事件类型 | 目标界面 | 切换界面 |
|----------|----------|----------|----------|
| settings_btn | UI_EVENT_NONE | UI_SCREEN_SETTINGS | true |
| unlock_btn | UI_EVENT_UNLOCK | UI_SCREEN_MAX | false |
| lock_btn | UI_EVENT_LOCK | UI_SCREEN_MAX | false |
| cancel_btn | UI_EVENT_COUNTDOWN_CANCEL | UI_SCREEN_MAIN | true |
| alarm_close_btn | UI_EVENT_ALARM_CONFIRM | UI_SCREEN_MAIN | true |
| back_btn | UI_EVENT_NONE | UI_SCREEN_MAIN | true |
| slider | UI_EVENT_SETTINGS_CHANGED | UI_SCREEN_MAX | false |
| switch | UI_EVENT_SETTINGS_CHANGED | UI_SCREEN_MAX | false |
| countdown_timer | UI_EVENT_COUNTDOWN_END | UI_SCREEN_MAIN | true |

---

## 代码统计

| 指标 | v2.3 | v2.4 | 变化 |
|------|------|------|------|
| 回调函数数量 | 10 | 5 | -50% |
| notify_ui_event 调用 | 6 | 0 | -100% |
| 直接调用 ui_switch_screen_internal | 3 | 0 | -100% |
| 直接调用业务回调 | 6 | 0 | -100% |

---

## 测试建议

### 1. 功能测试
- 测试所有按钮功能
- 测试倒计时功能
- 测试报警功能
- 测试设置功能

### 2. 并发测试
- 多线程同时触发事件
- 检查是否有死锁
- 检查状态一致性

### 3. 压力测试
- 快速连续触发事件
- 检查队列是否溢出
- 检查是否有内存泄漏

---

## 版本历史

- **v2.4.0**：架构统一，移除 notify_ui_event
- **v2.3.1**：修复回调重入风险（仍有架构问题）
- **v2.3.0**：使用环形缓冲区（有回调重入风险）
- **v2.2.0**：使用单个槽位（有事件丢失风险）
- **v2.1.0**：直接在定时器回调中切换界面（有跨锁风险）
