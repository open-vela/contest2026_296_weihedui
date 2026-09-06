# v2.3 修复事件丢失问题

## 问题描述

v2.2 中使用单个 `pending_action_t` 槽位存储待执行动作，如果多个事件快速触发，只有最后一个会被保留，之前的会丢失。

```c
// 问题：单个槽位
typedef struct {
    pending_action_t pending_action;  // 只能存一个
    // ...
} ui_manager_t;
```

**丢失场景示例**：
1. 倒计时结束 → 设置 `pending_action = COUNTDOWN_END`
2. 用户快速点击取消 → 设置 `pending_action = COUNTDOWN_CANCEL`
3. 结果：`COUNTDOWN_END` 事件丢失

## 修复方案

使用**环形缓冲区**替代单个槽位，可以存储多个待执行动作。

### 数据结构

```c
/* 待执行动作 */
typedef struct {
    ui_event_t event;
    void *data;
    ui_screen_t target_screen;
    bool switch_screen;
} pending_action_t;

/* 环形缓冲区 */
#define PENDING_ACTION_QUEUE_SIZE  8

typedef struct {
    pending_action_t items[PENDING_ACTION_QUEUE_SIZE];
    int head;   /* 读取位置 */
    int tail;   /* 写入位置 */
    int count;  /* 当前数量 */
} pending_action_queue_t;
```

### 队列操作

```c
/* 入队 */
static int pending_queue_push(ui_event_t event, void *data, 
                               ui_screen_t target, bool do_switch)
{
    if (pending_queue_is_full()) {
        /* 队列满时丢弃最旧的事件 */
        g_ui_manager.pending_queue.head = 
            (g_ui_manager.pending_queue.head + 1) % PENDING_ACTION_QUEUE_SIZE;
        g_ui_manager.pending_queue.count--;
    }

    /* 添加到队尾 */
    pending_action_queue_t *q = &g_ui_manager.pending_queue;
    q->items[q->tail].event = event;
    q->items[q->tail].data = data;
    q->items[q->tail].target_screen = target;
    q->items[q->tail].switch_screen = do_switch;
    q->tail = (q->tail + 1) % PENDING_ACTION_QUEUE_SIZE;
    q->count++;

    return 0;
}

/* 出队 */
static int pending_queue_pop(pending_action_t *action)
{
    if (pending_queue_is_empty()) {
        return -1;
    }

    pending_action_queue_t *q = &g_ui_manager.pending_queue;
    *action = q->items[q->head];
    q->head = (q->head + 1) % PENDING_ACTION_QUEUE_SIZE;
    q->count--;

    return 0;
}
```

### 处理所有待执行动作

```c
static int process_pending_action(void)
{
    pending_action_t action;
    int processed = 0;

    /* 处理队列中的所有动作 */
    while (pending_queue_pop(&action) == 0) {
        /* 先切换界面 */
        if (action.switch_screen && action.target_screen < UI_SCREEN_MAX) {
            ui_switch_screen_internal(action.target_screen);
        }

        /* 再通知事件 */
        if (action.event != UI_EVENT_NONE) {
            notify_ui_event(action.event, action.data);
        }

        processed++;
    }

    return processed;
}
```

## 修复对比

### 修复前（单槽位，会丢失事件）

```c
typedef struct {
    pending_action_t pending_action;  // 单个槽位
} ui_manager_t;

static void set_pending_action(...) {
    g_ui_manager.pending_action.event = event;      // 覆盖旧事件
    g_ui_manager.pending_action.valid = true;
}

static int process_pending_action(void) {
    if (!g_ui_manager.pending_action.valid) {
        return 0;  // 只处理一个
    }
    // 处理...
    g_ui_manager.pending_action.valid = false;
    return 0;
}
```

### 修复后（环形缓冲区，不会丢失事件）

```c
typedef struct {
    pending_action_queue_t pending_queue;  // 环形缓冲区
} ui_manager_t;

static void set_pending_action(...) {
    pending_queue_push(event, data, target, do_switch);  // 入队
}

static int process_pending_action(void) {
    pending_action_t action;
    int processed = 0;

    /* 处理所有待执行动作 */
    while (pending_queue_pop(&action) == 0) {
        // 处理...
        processed++;
    }

    return processed;  // 返回处理数量
}
```

## 优点

1. **不丢失事件**：最多可缓存 8 个待执行动作
2. **FIFO 顺序**：先发生先处理
3. **自动丢弃旧事件**：队列满时丢弃最旧的事件
4. **内存固定**：静态分配，无动态内存

## 队列大小

```c
#define PENDING_ACTION_QUEUE_SIZE  8
```

- 默认 8 个槽位
- 可根据实际需求调整
- 内存占用：`8 * sizeof(pending_action_t)` ≈ 80 字节

## 测试场景

### 场景 1：快速连续事件
```
1. 倒计时结束 → COUNTDOWN_END 入队
2. 用户点击取消 → COUNTDOWN_CANCEL 入队
3. 主循环处理 → 先处理 COUNTDOWN_END，再处理 COUNTDOWN_CANCEL
```

### 场景 2：队列满
```
1. 连续触发 8 个事件
2. 第 9 个事件触发
3. 丢弃第 1 个事件，保留第 2-9 个事件
```

### 场景 3：正常处理
```
1. 事件入队
2. 主循环调用 ui_manager_process()
3. 处理所有待执行动作
4. 队列清空
```

## 性能分析

- **入队操作**：O(1)
- **出队操作**：O(1)
- **内存占用**：固定 80 字节
- **锁持有时间**：极短（仅队列操作）

## 注意事项

1. **队列大小**：根据实际并发事件数量调整
2. **事件顺序**：保证 FIFO 顺序处理
3. **内存占用**：静态分配，无内存碎片
4. **线程安全**：所有操作都在锁保护下

## 版本历史

- **v2.3.0**：使用环形缓冲区替代单个槽位
- **v2.2.0**：使用单个槽位（有事件丢失风险）
- **v2.1.0**：直接在定时器回调中切换界面（有跨锁风险）
