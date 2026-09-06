# v2.2 问题修复说明

## 问题 1：ui_manager_deinit() 引用不存在成员

**问题描述**
`ui_manager_deinit()` 中引用了 `g_ui_manager.has_pending_action`，但该字段在 v2.2 中已改为 `pending_action.valid`。

**修复**
```c
// 修复前
g_ui_manager.has_pending_action = false;

// 修复后
g_ui_manager.pending_action.valid = false;
```

**文件位置**
`src/ui_manager.c:686`

---

## 问题 2：头文件缺少声明

**检查结果**
头文件 `ui_manager.h` 中已包含所需声明：

```c
// UI_EVENT_NONE 枚举值（第73行）
typedef enum {
    UI_EVENT_NONE = 0,       /* 无事件 */
    UI_EVENT_UNLOCK,
    // ...
} ui_event_t;

// ui_manager_process() 函数声明（第139行）
int ui_manager_process(void);
```

**状态**：✅ 无需修复

---

## 问题 3：unlock_btn_event_cb / lock_btn_event_cb 没有加锁保护

**问题描述**
开锁和锁定按钮的事件回调函数没有加锁保护，可能导致并发问题。

**修复**
```c
// 修复前
static void unlock_btn_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    notify_ui_event(UI_EVENT_UNLOCK, NULL);  // ⚠️ 无锁保护
}

static void lock_btn_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    notify_ui_event(UI_EVENT_LOCK, NULL);    // ⚠️ 无锁保护
}

// 修复后
static void unlock_btn_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_lock();                                // ✅ 加锁保护
    notify_ui_event(UI_EVENT_UNLOCK, NULL);
    ui_unlock();
}

static void lock_btn_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_lock();                                // ✅ 加锁保护
    notify_ui_event(UI_EVENT_LOCK, NULL);
    ui_unlock();
}
```

**文件位置**
`src/ui_manager.c:587-597`

---

## 修复统计

| 问题 | 状态 | 说明 |
|------|------|------|
| has_pending_action 引用错误 | ✅ 已修复 | 改为 pending_action.valid |
| 头文件缺少声明 | ✅ 无需修复 | 已包含所需声明 |
| 按钮回调无锁保护 | ✅ 已修复 | 添加 ui_lock/ui_unlock |

## 验证方法

### 1. 编译验证
```bash
make -j$(nproc)
```

### 2. 功能验证
- 测试开锁按钮
- 测试锁定按钮
- 测试倒计时结束自动切换
- 测试取消倒计时

### 3. 并发验证
- 多线程同时触发按钮事件
- 检查是否有死锁或竞态条件

## 代码审查清单

- [x] 所有公共 API 都有加锁保护
- [x] 所有定时器回调使用 trylock
- [x] 所有待执行动作使用 pending_action
- [x] 头文件声明完整
- [x] 结构体成员访问正确
