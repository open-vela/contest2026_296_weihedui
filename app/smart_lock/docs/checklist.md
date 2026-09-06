# UI 管理器集成检查清单

## 编译环境配置

- [ ] NuttX 开启 Pthread：`CONFIG_PTHREAD=y`
- [ ] LVGL 字体配置：`lv_font_montserrat_24/36/48`
- [ ] Makefile/CMake 添加源文件和头文件路径

## 源文件列表

- [ ] `app/smart_lock/src/ui_manager.c` - UI 管理器实现
- [ ] `app/smart_lock/include/ui_manager.h` - UI 管理器头文件

## 头文件路径

```makefile
CFLAGS += -I$(APPDIR)/app/smart_lock/include
```

## 依赖库

```makefile
LDFLAGS += -lpthread
```

## 业务层集成

### 1. 实现 UI 事件回调函数

```c
void ui_event_handler(ui_event_t event, void *data, void *user_data)
{
    switch (event) {
        case UI_EVENT_UNLOCK:
            // 业务：开锁
            break;
        case UI_EVENT_LOCK:
            // 业务：锁定
            break;
        case UI_EVENT_COUNTDOWN_END:
            // 倒计时结束
            break;
        case UI_EVENT_COUNTDOWN_CANCEL:
            // 倒计时取消
            break;
        case UI_EVENT_ALARM_CONFIRM:
            // 用户确认报警
            break;
        case UI_EVENT_SETTINGS_CHANGED:
            // 设置参数变更
            break;
        default:
            break;
    }
}
```

**注意事项**：
- [ ] 只做消息转发，禁止 sleep、阻塞
- [ ] 快速返回，不要执行耗时操作
- [ ] 不要在回调中调用 `ui_manager_deinit()`

### 2. 系统初始化

```c
// 初始化 UI 管理器
ui_manager_init();

// 注册事件回调
ui_register_event_callback(ui_event_handler, NULL);
```

**调用时机**：
- [ ] 系统启动时调用一次
- [ ] 在主循环之前调用

### 3. 主循环

```c
while (1) {
    ui_manager_process();   // ⚠️ 关键！
    lv_timer_handler();
    usleep(10000);          // 10ms
}
```

**注意事项**：
- [ ] 必须周期性调用 `ui_manager_process()`
- [ ] 推荐频率：100Hz（10ms 间隔）
- [ ] `lv_timer_handler()` 也要定期调用

### 4. 退出反初始化

**正确流程**：
1. [ ] 停止运行 `ui_manager_process()` 的任务/主循环
2. [ ] join 等待该任务完全退出
3. [ ] 再调用 `ui_manager_deinit()`

**错误做法**：
```c
// ❌ 错误：直接调用 deinit
ui_manager_deinit();  // 危险！可能崩溃或死锁
```

**正确做法**：
```c
// ✅ 正确：先停止主循环
running = false;
pthread_join(ui_thread, NULL);  // 等待线程退出
ui_manager_deinit();            // 再调用 deinit
```

## NuttX 配置

```kconfig
# 必须开启
CONFIG_PTHREAD=y
CONFIG_PTHREAD_MUTEX=y
CONFIG_PTHREAD_COND=y

# LVGL 字体
CONFIG_LV_FONT_MONTSERRAT_24=y
CONFIG_LV_FONT_MONTSERRAT_36=y
CONFIG_LV_FONT_MONTSERRAT_48=y
```

## Makefile 配置

```makefile
# 源文件
CSRCS += app/smart_lock/src/ui_manager.c

# 头文件路径
CFLAGS += -I$(APPDIR)/app/smart_lock/include

# 依赖库
LDFLAGS += -lpthread
```

## 常见问题

### Q: 编译报错：undefined reference to `pthread_mutex_init`
**原因**：NuttX 未开启 Pthread 支持
**解决**：启用 `CONFIG_PTHREAD=y`

### Q: 界面文字显示乱码或不显示
**原因**：LVGL 字体未启用
**解决**：启用 `CONFIG_LV_FONT_MONTSERRAT_24/36/48`

### Q: 程序启动后立即崩溃
**原因**：`ui_manager_process()` 未在主循环中调用
**解决**：确保主循环中定期调用 `ui_manager_process()`

### Q: 退出时程序卡死
**原因**：直接调用 `ui_manager_deinit()`，未停止主循环
**解决**：先停止主循环，再调用 `ui_manager_deinit()`

### Q: 事件回调不执行
**原因**：`ui_manager_process()` 未被调用
**解决**：确保主循环中定期调用 `ui_manager_process()`

## 调试技巧

### 启用日志

```c
#define UI_LOG_LEVEL LOG_DEBUG
```

### 监控 pending 队列

```c
int processed = ui_manager_process();
if (processed > 0) {
    printf("Processed %d events\n", processed);
}
```

### 监控引用计数

```c
if (g_ui_manager.callback_refcount > 0) {
    printf("Callback refcount: %d\n", g_ui_manager.callback_refcount);
}
```

## 性能优化

### 减少锁持有时间

```c
ui_lock();
// 只做必要的状态更新
ui_unlock();
// 耗时操作放在锁外
```

### 减少回调执行时间

```c
void ui_event_handler(ui_event_t event, void *data, void *user_data)
{
    /* 发送到消息队列，不在回调中直接处理 */
    msg_queue_send(event, data);
}
```

### 调整主循环频率

```c
#define UI_PROCESS_INTERVAL_MS  10  /* 100Hz */
// 或
#define UI_PROCESS_INTERVAL_MS  20  /* 50Hz */
```

---

## 版本信息

- **UI 管理器版本**：v2.4.5
- **最后更新**：2024-01-01
- **许可证**：Apache License 2.0
