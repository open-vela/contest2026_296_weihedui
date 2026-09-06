/**
 * @file ui_manager.c
 * @brief 智能锁 LVGL 界面管理器（v2.4 架构统一版）
 * @version 2.4.0
 * @date 2024-01-01
 *
 * 修复内容：
 * 1. 移除 notify_ui_event 锁释放-重获取设计（消除死锁风险）
 * 2. 所有 LVGL 回调统一走 pending 队列
 * 3. 所有界面切换统一走 pending 队列
 * 4. 修复 strftime() 返回值检查
 */

#include <nuttx/config.h>
#include <lvgl/lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>

#include "ui_manager.h"

#define TAG "UI_MGR"
#define UI_LOGE(fmt, ...) syslog(LOG_ERR, "[%s] " fmt "\n", TAG, ##__VA_ARGS__)
#define UI_LOGW(fmt, ...) syslog(LOG_WARNING, "[%s] " fmt "\n", TAG, ##__VA_ARGS__)
#define UI_LOGI(fmt, ...) syslog(LOG_INFO, "[%s] " fmt "\n", TAG, ##__VA_ARGS__)
#define UI_LOGD(fmt, ...) syslog(LOG_DEBUG, "[%s] " fmt "\n", TAG, ##__VA_ARGS__)

#define SCREEN_NOT_CREATED  0
#define SCREEN_CREATED      1

/* 控件结构体 */
typedef struct {
    lv_obj_t *screen, *top_bar, *time_label, *battery_icon, *wifi_icon;
    lv_obj_t *status_icon, *door_label, *lock_label;
    lv_obj_t *btn_container, *settings_btn, *unlock_btn, *lock_btn;
} main_screen_t;

typedef struct {
    lv_obj_t *screen, *warning_icon, *countdown_label, *countdown_bar;
    lv_obj_t *cancel_btn, *cancel_btn_label, *hint_label;
} countdown_screen_t;

typedef struct {
    lv_obj_t *screen, *alarm_icon, *alarm_label, *alarm_detail;
    lv_obj_t *close_btn, *close_btn_label;
    bool anim_active;
} alarm_screen_t;

typedef struct {
    lv_obj_t *screen, *title_bar, *back_btn, *back_btn_label, *title_label;
    lv_obj_t *settings_list, *close_time_container, *close_time_title, *close_time_slider;
    lv_obj_t *volume_container, *volume_title, *volume_slider;
    lv_obj_t *brightness_container, *brightness_title, *brightness_slider;
    lv_obj_t *antipinch_container, *antipinch_title, *antipinch_switch;
} settings_screen_t;

/* 待执行动作 */
typedef struct {
    ui_event_t event;
    void *data;
    ui_screen_t target_screen;
    bool switch_screen;
} pending_action_t;

/* 待执行动作队列（环形缓冲区） */
#define PENDING_ACTION_QUEUE_SIZE  8

typedef struct {
    pending_action_t items[PENDING_ACTION_QUEUE_SIZE];
    int head;  /* 读取位置 */
    int tail;  /* 写入位置 */
    int count; /* 当前数量 */
} pending_action_queue_t;

/* 管理器主结构体 */
typedef struct {
    main_screen_t main_screen;
    countdown_screen_t countdown_screen;
    alarm_screen_t alarm_screen;
    settings_screen_t settings_screen;
    uint8_t screen_created[UI_SCREEN_MAX];
    ui_screen_t current_screen;
    lv_timer_t *time_update_timer;
    lv_timer_t *countdown_timer;
    door_status_t door_status;
    lock_status_t lock_status;
    system_status_t system_status;
    int countdown_seconds;
    int countdown_total;
    ui_settings_t settings;
    ui_event_cb_t event_callback;
    void *event_user_data;
    pending_action_queue_t pending_queue;
    pthread_mutex_t mutex;
    pthread_cond_t callback_done;    /* 条件变量：回调执行完成 */
    volatile bool initialized;
    volatile bool deinitializing;
    volatile int callback_refcount;  /* 回调执行引用计数，保护 mutex 生命周期 */
} ui_manager_t;

static ui_manager_t g_ui_manager;

/* 内部函数声明 */
static int create_main_screen(void);
static int create_countdown_screen(void);
static int create_alarm_screen(void);
static int create_settings_screen(void);
static void destroy_main_screen(void);
static void destroy_countdown_screen(void);
static void destroy_alarm_screen(void);
static void destroy_settings_screen(void);
static int ui_switch_screen_internal(ui_screen_t screen);

/* 回调函数声明 */
static void update_time_label_cb(lv_timer_t *timer);
static void countdown_timer_cb(lv_timer_t *timer);
static void btn_event_cb(lv_event_t *e);
static void slider_event_cb(lv_event_t *e);
static void switch_event_cb(lv_event_t *e);

static inline int ui_lock(void) { return pthread_mutex_lock(&g_ui_manager.mutex); }
static inline int ui_trylock(void) { return pthread_mutex_trylock(&g_ui_manager.mutex); }
static inline int ui_unlock(void) { return pthread_mutex_unlock(&g_ui_manager.mutex); }
static inline bool is_valid_obj(lv_obj_t *obj) { return obj && lv_obj_is_valid(obj); }

/*
 * 锁管理约定（v2.4.5 最终版）：
 *
 * 1. ui_manager_process() - 锁契约简单明确
 *    - 进入时：通过 trylock 获取锁（最多重试 3 次）
 *    - 返回时：锁被释放
 *    - 内部流程：
 *      a. trylock 获取锁（最多重试 3 次）
 *      b. 复制动作、执行界面切换（在锁内）
 *      c. callback_refcount++（保护 mutex 生命周期）
 *      d. 释放锁
 *      e. 执行回调（在锁外）
 *      f. 重新获取锁
 *      g. callback_refcount--，保存 should_signal
 *      h. 释放锁
 *      i. 如果 should_signal，在锁外 pthread_cond_signal
 *
 * 2. LVGL 回调（btn_event_cb, slider_event_cb 等）
 *    - trylock 获取锁
 *    - set_pending_action() 添加到队列
 *    - unlock 释放锁
 *
 * 3. mutex 生命周期保护（callback_refcount + 条件变量）
 *    - ui_manager_process() 释放锁前：callback_refcount++
 *    - ui_manager_process() 重新获取锁后：callback_refcount--
 *    - 如果 callback_refcount == 0，在锁外 pthread_cond_signal
 *    - ui_manager_deinit() 使用 pthread_cond_wait 等待 callback_refcount == 0
 *    - 确保回调执行期间 mutex 不会被销毁
 *
 * 4. pthread_cond_signal 在锁外调用
 *    - 避免在持有锁时唤醒等待线程（惊群效应）
 *    - 先保存 should_signal 标志，释放锁后再发送信号
 *
 * 5. ui_trylock() 非阻塞特性
 *    - ui_manager_process() 使用 ui_trylock() 避免阻塞主循环
 *    - 最多重试 3 次，每次间隔 100us
 *    - 如果 3 次都失败，返回 0，队列不消费
 *    - 事件会积压在 pending 队列，下次调用时处理
 *    - 这是设计取舍：避免阻塞 vs 及时处理
 */

/* ==================== 队列操作函数 ==================== */

static bool pending_queue_is_empty(void)
{
    return g_ui_manager.pending_queue.count == 0;
}

static bool pending_queue_is_full(void)
{
    return g_ui_manager.pending_queue.count >= PENDING_ACTION_QUEUE_SIZE;
}

static int pending_queue_push(ui_event_t event, void *data, ui_screen_t target, bool do_switch)
{
    if (pending_queue_is_full()) {
        UI_LOGW("Pending action queue full, dropping oldest action");
        g_ui_manager.pending_queue.head = (g_ui_manager.pending_queue.head + 1) % PENDING_ACTION_QUEUE_SIZE;
        g_ui_manager.pending_queue.count--;
    }

    pending_action_queue_t *q = &g_ui_manager.pending_queue;
    q->items[q->tail].event = event;
    q->items[q->tail].data = data;
    q->items[q->tail].target_screen = target;
    q->items[q->tail].switch_screen = do_switch;
    q->tail = (q->tail + 1) % PENDING_ACTION_QUEUE_SIZE;
    q->count++;

    UI_LOGD("Pending action pushed: event=%d target=%d count=%d", event, target, q->count);
    return 0;
}

static int pending_queue_pop(pending_action_t *action)
{
    if (pending_queue_is_empty()) {
        return -1;
    }

    pending_action_queue_t *q = &g_ui_manager.pending_queue;
    *action = q->items[q->head];
    q->head = (q->head + 1) % PENDING_ACTION_QUEUE_SIZE;
    q->count--;

    UI_LOGD("Pending action popped: event=%d count=%d", action->event, q->count);
    return 0;
}

/* 设置待执行动作（调用时已持有锁） */
static void set_pending_action(ui_event_t event, void *data, ui_screen_t target, bool do_switch)
{
    pending_queue_push(event, data, target, do_switch);
}

/*
 * 从队列复制待执行动作（调用时已持有锁）
 * 返回值：复制的动作数量
 */
static int copy_pending_actions(pending_action_t *actions, int max_count)
{
    int count = 0;
    while (count < max_count && !pending_queue_is_empty()) {
        if (pending_queue_pop(&actions[count]) == 0) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

/* ==================== 回调函数 ==================== */

/* 时间更新回调 */
static void update_time_label_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (ui_trylock() != 0) return;
    if (g_ui_manager.deinitializing) { ui_unlock(); return; }

    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);

    char buf[6];
    /* 修复：检查 strftime 返回值 */
    size_t ret = strftime(buf, sizeof(buf), "%H:%M", tm_info);
    if (ret == 0) {
        UI_LOGW("strftime failed or buffer too small");
        ui_unlock();
        return;
    }

    if (is_valid_obj(g_ui_manager.main_screen.time_label)) {
        lv_label_set_text(g_ui_manager.main_screen.time_label, buf);
    }
    ui_unlock();
}

/* 倒计时回调 */
static void countdown_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (ui_trylock() != 0) return;
    if (g_ui_manager.deinitializing) { ui_unlock(); return; }

    g_ui_manager.countdown_seconds--;

    if (g_ui_manager.countdown_seconds <= 0) {
        if (g_ui_manager.countdown_timer) {
            lv_timer_del(g_ui_manager.countdown_timer);
            g_ui_manager.countdown_timer = NULL;
        }
        /* 统一走 pending 队列 */
        set_pending_action(UI_EVENT_COUNTDOWN_END, NULL, UI_SCREEN_MAIN, true);
        UI_LOGI("Countdown finished, pending switch to main");
        ui_unlock();
        return;
    }

    if (is_valid_obj(g_ui_manager.countdown_screen.countdown_label)) {
        lv_label_set_text_fmt(g_ui_manager.countdown_screen.countdown_label, "%d秒后自动关门", g_ui_manager.countdown_seconds);
    }
    if (is_valid_obj(g_ui_manager.countdown_screen.countdown_bar)) {
        lv_bar_set_value(g_ui_manager.countdown_screen.countdown_bar, g_ui_manager.countdown_seconds, LV_ANIM_ON);
    }
    ui_unlock();
}

/* 统一按钮事件回调 */
static void btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    LV_UNUSED(e);

    if (ui_trylock() != 0) return;

    /* 根据按钮对象判断事件类型 */
    if (btn == g_ui_manager.main_screen.settings_btn) {
        /* 设置按钮：切换到设置界面 */
        set_pending_action(UI_EVENT_NONE, NULL, UI_SCREEN_SETTINGS, true);
    } else if (btn == g_ui_manager.main_screen.unlock_btn) {
        /* 开锁按钮：通知开锁事件 */
        set_pending_action(UI_EVENT_UNLOCK, NULL, UI_SCREEN_MAX, false);
    } else if (btn == g_ui_manager.main_screen.lock_btn) {
        /* 锁定按钮：通知锁定事件 */
        set_pending_action(UI_EVENT_LOCK, NULL, UI_SCREEN_MAX, false);
    } else if (btn == g_ui_manager.countdown_screen.cancel_btn) {
        /* 取消按钮：取消倒计时 */
        if (g_ui_manager.countdown_timer) {
            lv_timer_del(g_ui_manager.countdown_timer);
            g_ui_manager.countdown_timer = NULL;
        }
        set_pending_action(UI_EVENT_COUNTDOWN_CANCEL, NULL, UI_SCREEN_MAIN, true);
    } else if (btn == g_ui_manager.alarm_screen.close_btn) {
        /* 报警确认按钮 */
        if (is_valid_obj(g_ui_manager.alarm_screen.alarm_icon)) {
            lv_anim_del(g_ui_manager.alarm_screen.alarm_icon, NULL);
        }
        g_ui_manager.alarm_screen.anim_active = false;
        set_pending_action(UI_EVENT_ALARM_CONFIRM, NULL, UI_SCREEN_MAIN, true);
    } else if (btn == g_ui_manager.settings_screen.back_btn) {
        /* 返回按钮 */
        set_pending_action(UI_EVENT_NONE, NULL, UI_SCREEN_MAIN, true);
    }

    ui_unlock();
}

/* 滑块事件回调 */
static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    int type = (int)(intptr_t)lv_event_get_user_data(e);

    if (ui_trylock() != 0) return;

    switch (type) {
        case 0: g_ui_manager.settings.auto_close_time = val; break;
        case 1: g_ui_manager.settings.voice_volume = val; break;
        case 2: g_ui_manager.settings.screen_brightness = val; break;
    }

    /* 统一走 pending 队列 */
    set_pending_action(UI_EVENT_SETTINGS_CHANGED, (void*)(intptr_t)type, UI_SCREEN_MAX, false);
    ui_unlock();
}

/* 开关事件回调 */
static void switch_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if (ui_trylock() != 0) return;

    if (is_valid_obj(g_ui_manager.settings_screen.antipinch_switch)) {
        g_ui_manager.settings.antipinch_enabled = lv_obj_has_state(g_ui_manager.settings_screen.antipinch_switch, LV_STATE_CHECKED);
    }

    /* 统一走 pending 队列 */
    set_pending_action(UI_EVENT_SETTINGS_CHANGED, (void*)(intptr_t)3, UI_SCREEN_MAX, false);
    ui_unlock();
}

/* ==================== 界面创建/销毁 ==================== */

static int create_main_screen(void)
{
    if (g_ui_manager.screen_created[UI_SCREEN_MAIN] == SCREEN_CREATED) return 0;

    main_screen_t *s = &g_ui_manager.main_screen;

    s->screen = lv_obj_create(NULL);
    if (!is_valid_obj(s->screen)) return -1;
    lv_obj_set_style_bg_color(s->screen, lv_color_hex(0x1a1a2e), 0);

    s->top_bar = lv_obj_create(s->screen);
    if (!is_valid_obj(s->top_bar)) { destroy_main_screen(); return -1; }
    lv_obj_set_size(s->top_bar, LV_PCT(100), 40);
    lv_obj_align(s->top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s->top_bar, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(s->top_bar, 0, 0);
    lv_obj_set_style_radius(s->top_bar, 0, 0);

    s->time_label = lv_label_create(s->top_bar);
    if (!is_valid_obj(s->time_label)) { destroy_main_screen(); return -1; }
    lv_label_set_text(s->time_label, "12:00");
    lv_obj_align(s->time_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(s->time_label, lv_color_hex(0xffffff), 0);

    s->battery_icon = lv_label_create(s->top_bar);
    if (!is_valid_obj(s->battery_icon)) { destroy_main_screen(); return -1; }
    lv_label_set_text(s->battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_align(s->battery_icon, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(s->battery_icon, lv_color_hex(0x00ff00), 0);

    s->wifi_icon = lv_label_create(s->top_bar);
    if (!is_valid_obj(s->wifi_icon)) { destroy_main_screen(); return -1; }
    lv_label_set_text(s->wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(s->wifi_icon, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_obj_set_style_text_color(s->wifi_icon, lv_color_hex(0x00ff00), 0);

    s->status_icon = lv_label_create(s->screen);
    if (!is_valid_obj(s->status_icon)) { destroy_main_screen(); return -1; }
    lv_label_set_text(s->status_icon, LV_SYMBOL_LOCK);
    lv_obj_align(s->status_icon, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_text_color(s->status_icon, lv_color_hex(0x00ff00), 0);
    lv_obj_set_style_text_font(s->status_icon, &lv_font_montserrat_48, 0);

    s->door_label = lv_label_create(s->screen);
    if (!is_valid_obj(s->door_label)) { destroy_main_screen(); return -1; }
    lv_label_set_text(s->door_label, "门: 关闭");
    lv_obj_align(s->door_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(s->door_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s->door_label, &lv_font_montserrat_24, 0);

    s->lock_label = lv_label_create(s->screen);
    if (!is_valid_obj(s->lock_label)) { destroy_main_screen(); return -1; }
    lv_label_set_text(s->lock_label, "锁: 已锁定");
    lv_obj_align(s->lock_label, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_color(s->lock_label, lv_color_hex(0xffffff), 0);

    s->btn_container = lv_obj_create(s->screen);
    if (!is_valid_obj(s->btn_container)) { destroy_main_screen(); return -1; }
    lv_obj_set_size(s->btn_container, LV_PCT(90), 60);
    lv_obj_align(s->btn_container, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(s->btn_container, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(s->btn_container, 0, 0);
    lv_obj_set_style_radius(s->btn_container, 15, 0);
    lv_obj_set_flex_flow(s->btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s->btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 统一使用 btn_event_cb */
    s->settings_btn = lv_btn_create(s->btn_container);
    if (!is_valid_obj(s->settings_btn)) { destroy_main_screen(); return -1; }
    lv_obj_set_size(s->settings_btn, 80, 40);
    lv_obj_set_style_bg_color(s->settings_btn, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_radius(s->settings_btn, 10, 0);
    lv_obj_t *lbl = lv_label_create(s->settings_btn);
    if (is_valid_obj(lbl)) { lv_label_set_text(lbl, LV_SYMBOL_SETTINGS); lv_obj_center(lbl); }
    lv_obj_add_event_cb(s->settings_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    s->unlock_btn = lv_btn_create(s->btn_container);
    if (!is_valid_obj(s->unlock_btn)) { destroy_main_screen(); return -1; }
    lv_obj_set_size(s->unlock_btn, 80, 40);
    lv_obj_set_style_bg_color(s->unlock_btn, lv_color_hex(0x00b4d8), 0);
    lv_obj_set_style_radius(s->unlock_btn, 10, 0);
    lbl = lv_label_create(s->unlock_btn);
    if (is_valid_obj(lbl)) { lv_label_set_text(lbl, "开锁"); lv_obj_center(lbl); }
    lv_obj_add_event_cb(s->unlock_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    s->lock_btn = lv_btn_create(s->btn_container);
    if (!is_valid_obj(s->lock_btn)) { destroy_main_screen(); return -1; }
    lv_obj_set_size(s->lock_btn, 80, 40);
    lv_obj_set_style_bg_color(s->lock_btn, lv_color_hex(0xe63946), 0);
    lv_obj_set_style_radius(s->lock_btn, 10, 0);
    lbl = lv_label_create(s->lock_btn);
    if (is_valid_obj(lbl)) { lv_label_set_text(lbl, "锁定"); lv_obj_center(lbl); }
    lv_obj_add_event_cb(s->lock_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    g_ui_manager.screen_created[UI_SCREEN_MAIN] = SCREEN_CREATED;
    return 0;
}

static void destroy_main_screen(void)
{
    if (is_valid_obj(g_ui_manager.main_screen.screen)) {
        lv_obj_del(g_ui_manager.main_screen.screen);
    }
    memset(&g_ui_manager.main_screen, 0, sizeof(main_screen_t));
    g_ui_manager.screen_created[UI_SCREEN_MAIN] = SCREEN_NOT_CREATED;
}

static int create_countdown_screen(void)
{
    if (g_ui_manager.screen_created[UI_SCREEN_COUNTDOWN] == SCREEN_CREATED) return 0;

    countdown_screen_t *s = &g_ui_manager.countdown_screen;

    s->screen = lv_obj_create(NULL);
    if (!is_valid_obj(s->screen)) return -1;
    lv_obj_set_style_bg_color(s->screen, lv_color_hex(0x1a1a2e), 0);

    s->warning_icon = lv_label_create(s->screen);
    if (!is_valid_obj(s->warning_icon)) { destroy_countdown_screen(); return -1; }
    lv_label_set_text(s->warning_icon, LV_SYMBOL_WARNING);
    lv_obj_align(s->warning_icon, LV_ALIGN_CENTER, 0, -80);
    lv_obj_set_style_text_color(s->warning_icon, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_text_font(s->warning_icon, &lv_font_montserrat_48, 0);

    s->countdown_label = lv_label_create(s->screen);
    if (!is_valid_obj(s->countdown_label)) { destroy_countdown_screen(); return -1; }
    lv_label_set_text(s->countdown_label, "10秒后自动关门");
    lv_obj_align(s->countdown_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_color(s->countdown_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s->countdown_label, &lv_font_montserrat_24, 0);

    s->countdown_bar = lv_bar_create(s->screen);
    if (!is_valid_obj(s->countdown_bar)) { destroy_countdown_screen(); return -1; }
    lv_obj_set_size(s->countdown_bar, 250, 20);
    lv_obj_align(s->countdown_bar, LV_ALIGN_CENTER, 0, 20);
    lv_bar_set_range(s->countdown_bar, 0, g_ui_manager.settings.auto_close_time);
    lv_bar_set_value(s->countdown_bar, g_ui_manager.settings.auto_close_time, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s->countdown_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(s->countdown_bar, lv_color_hex(0x00b4d8), LV_PART_INDICATOR);

    /* 统一使用 btn_event_cb */
    s->cancel_btn = lv_btn_create(s->screen);
    if (!is_valid_obj(s->cancel_btn)) { destroy_countdown_screen(); return -1; }
    lv_obj_set_size(s->cancel_btn, 120, 50);
    lv_obj_align(s->cancel_btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(s->cancel_btn, lv_color_hex(0xe63946), 0);
    lv_obj_set_style_radius(s->cancel_btn, 15, 0);
    s->cancel_btn_label = lv_label_create(s->cancel_btn);
    if (is_valid_obj(s->cancel_btn_label)) { lv_label_set_text(s->cancel_btn_label, "取消关门"); lv_obj_center(s->cancel_btn_label); }
    lv_obj_add_event_cb(s->cancel_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    s->hint_label = lv_label_create(s->screen);
    if (!is_valid_obj(s->hint_label)) { destroy_countdown_screen(); return -1; }
    lv_label_set_text(s->hint_label, "门将在倒计时结束后自动关闭");
    lv_obj_align(s->hint_label, LV_ALIGN_CENTER, 0, 130);
    lv_obj_set_style_text_color(s->hint_label, lv_color_hex(0x888888), 0);

    g_ui_manager.screen_created[UI_SCREEN_COUNTDOWN] = SCREEN_CREATED;
    return 0;
}

static void destroy_countdown_screen(void)
{
    if (g_ui_manager.countdown_timer) { lv_timer_del(g_ui_manager.countdown_timer); g_ui_manager.countdown_timer = NULL; }
    if (is_valid_obj(g_ui_manager.countdown_screen.screen)) { lv_obj_del(g_ui_manager.countdown_screen.screen); }
    memset(&g_ui_manager.countdown_screen, 0, sizeof(countdown_screen_t));
    g_ui_manager.screen_created[UI_SCREEN_COUNTDOWN] = SCREEN_NOT_CREATED;
}

static int create_alarm_screen(void)
{
    if (g_ui_manager.screen_created[UI_SCREEN_ALERT] == SCREEN_CREATED) return 0;

    alarm_screen_t *s = &g_ui_manager.alarm_screen;

    s->screen = lv_obj_create(NULL);
    if (!is_valid_obj(s->screen)) return -1;
    lv_obj_set_style_bg_color(s->screen, lv_color_hex(0x1a1a2e), 0);

    s->alarm_icon = lv_label_create(s->screen);
    if (!is_valid_obj(s->alarm_icon)) { destroy_alarm_screen(); return -1; }
    lv_label_set_text(s->alarm_icon, LV_SYMBOL_WARNING);
    lv_obj_align(s->alarm_icon, LV_ALIGN_CENTER, 0, -80);
    lv_obj_set_style_text_color(s->alarm_icon, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_text_font(s->alarm_icon, &lv_font_montserrat_48, 0);

    s->alarm_label = lv_label_create(s->screen);
    if (!is_valid_obj(s->alarm_label)) { destroy_alarm_screen(); return -1; }
    lv_label_set_text(s->alarm_label, "报警！");
    lv_obj_align(s->alarm_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_color(s->alarm_label, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_text_font(s->alarm_label, &lv_font_montserrat_36, 0);

    s->alarm_detail = lv_label_create(s->screen);
    if (!is_valid_obj(s->alarm_detail)) { destroy_alarm_screen(); return -1; }
    lv_label_set_text(s->alarm_detail, "检测到异常");
    lv_obj_align(s->alarm_detail, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_text_color(s->alarm_detail, lv_color_hex(0xffffff), 0);

    /* 统一使用 btn_event_cb */
    s->close_btn = lv_btn_create(s->screen);
    if (!is_valid_obj(s->close_btn)) { destroy_alarm_screen(); return -1; }
    lv_obj_set_size(s->close_btn, 120, 50);
    lv_obj_align(s->close_btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(s->close_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(s->close_btn, 15, 0);
    s->close_btn_label = lv_label_create(s->close_btn);
    if (is_valid_obj(s->close_btn_label)) { lv_label_set_text(s->close_btn_label, "确认"); lv_obj_center(s->close_btn_label); }
    lv_obj_add_event_cb(s->close_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    s->anim_active = false;
    g_ui_manager.screen_created[UI_SCREEN_ALERT] = SCREEN_CREATED;
    return 0;
}

static void destroy_alarm_screen(void)
{
    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_icon)) { lv_anim_del(g_ui_manager.alarm_screen.alarm_icon, NULL); }
    if (is_valid_obj(g_ui_manager.alarm_screen.screen)) { lv_obj_del(g_ui_manager.alarm_screen.screen); }
    memset(&g_ui_manager.alarm_screen, 0, sizeof(alarm_screen_t));
    g_ui_manager.screen_created[UI_SCREEN_ALERT] = SCREEN_NOT_CREATED;
}

static int create_settings_screen(void)
{
    if (g_ui_manager.screen_created[UI_SCREEN_SETTINGS] == SCREEN_CREATED) return 0;

    settings_screen_t *s = &g_ui_manager.settings_screen;

    s->screen = lv_obj_create(NULL);
    if (!is_valid_obj(s->screen)) return -1;
    lv_obj_set_style_bg_color(s->screen, lv_color_hex(0x1a1a2e), 0);

    s->title_bar = lv_obj_create(s->screen);
    if (!is_valid_obj(s->title_bar)) { destroy_settings_screen(); return -1; }
    lv_obj_set_size(s->title_bar, LV_PCT(100), 50);
    lv_obj_align(s->title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s->title_bar, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(s->title_bar, 0, 0);

    /* 统一使用 btn_event_cb */
    s->back_btn = lv_btn_create(s->title_bar);
    if (!is_valid_obj(s->back_btn)) { destroy_settings_screen(); return -1; }
    lv_obj_set_size(s->back_btn, 40, 40);
    lv_obj_align(s->back_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(s->back_btn, lv_color_hex(0x0f3460), 0);
    s->back_btn_label = lv_label_create(s->back_btn);
    if (is_valid_obj(s->back_btn_label)) { lv_label_set_text(s->back_btn_label, LV_SYMBOL_LEFT); lv_obj_center(s->back_btn_label); }
    lv_obj_add_event_cb(s->back_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    s->title_label = lv_label_create(s->title_bar);
    if (!is_valid_obj(s->title_label)) { destroy_settings_screen(); return -1; }
    lv_label_set_text(s->title_label, "设置");
    lv_obj_center(s->title_label);
    lv_obj_set_style_text_color(s->title_label, lv_color_hex(0xffffff), 0);

    s->settings_list = lv_obj_create(s->screen);
    if (!is_valid_obj(s->settings_list)) { destroy_settings_screen(); return -1; }
    lv_obj_set_size(s->settings_list, LV_PCT(90), LV_PCT(80));
    lv_obj_align(s->settings_list, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(s->settings_list, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(s->settings_list, 0, 0);
    lv_obj_set_flex_flow(s->settings_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s->settings_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s->settings_list, 15, 0);
    lv_obj_set_style_pad_all(s->settings_list, 20, 0);

    /* 自动关门时间 */
    s->close_time_container = lv_obj_create(s->settings_list);
    if (!is_valid_obj(s->close_time_container)) { destroy_settings_screen(); return -1; }
    lv_obj_set_size(s->close_time_container, LV_PCT(100), 80);
    lv_obj_set_style_bg_color(s->close_time_container, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(s->close_time_container, 0, 0);
    s->close_time_title = lv_label_create(s->close_time_container);
    if (is_valid_obj(s->close_time_title)) { lv_label_set_text(s->close_time_title, "自动关门时间"); lv_obj_align(s->close_time_title, LV_ALIGN_TOP_LEFT, 10, 10); lv_obj_set_style_text_color(s->close_time_title, lv_color_hex(0xffffff), 0); }
    s->close_time_slider = lv_slider_create(s->close_time_container);
    if (is_valid_obj(s->close_time_slider)) { lv_obj_set_width(s->close_time_slider, LV_PCT(80)); lv_obj_align(s->close_time_slider, LV_ALIGN_BOTTOM_MID, 0, -15); lv_slider_set_range(s->close_time_slider, 5, 30); lv_slider_set_value(s->close_time_slider, g_ui_manager.settings.auto_close_time, LV_ANIM_OFF); lv_obj_add_event_cb(s->close_time_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)0); }

    /* 语音音量 */
    s->volume_container = lv_obj_create(s->settings_list);
    if (!is_valid_obj(s->volume_container)) { destroy_settings_screen(); return -1; }
    lv_obj_set_size(s->volume_container, LV_PCT(100), 80);
    lv_obj_set_style_bg_color(s->volume_container, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(s->volume_container, 0, 0);
    s->volume_title = lv_label_create(s->volume_container);
    if (is_valid_obj(s->volume_title)) { lv_label_set_text(s->volume_title, "语音音量"); lv_obj_align(s->volume_title, LV_ALIGN_TOP_LEFT, 10, 10); lv_obj_set_style_text_color(s->volume_title, lv_color_hex(0xffffff), 0); }
    s->volume_slider = lv_slider_create(s->volume_container);
    if (is_valid_obj(s->volume_slider)) { lv_obj_set_width(s->volume_slider, LV_PCT(80)); lv_obj_align(s->volume_slider, LV_ALIGN_BOTTOM_MID, 0, -15); lv_slider_set_range(s->volume_slider, 0, 100); lv_slider_set_value(s->volume_slider, g_ui_manager.settings.voice_volume, LV_ANIM_OFF); lv_obj_add_event_cb(s->volume_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)1); }

    /* 屏幕亮度 */
    s->brightness_container = lv_obj_create(s->settings_list);
    if (!is_valid_obj(s->brightness_container)) { destroy_settings_screen(); return -1; }
    lv_obj_set_size(s->brightness_container, LV_PCT(100), 80);
    lv_obj_set_style_bg_color(s->brightness_container, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(s->brightness_container, 0, 0);
    s->brightness_title = lv_label_create(s->brightness_container);
    if (is_valid_obj(s->brightness_title)) { lv_label_set_text(s->brightness_title, "屏幕亮度"); lv_obj_align(s->brightness_title, LV_ALIGN_TOP_LEFT, 10, 10); lv_obj_set_style_text_color(s->brightness_title, lv_color_hex(0xffffff), 0); }
    s->brightness_slider = lv_slider_create(s->brightness_container);
    if (is_valid_obj(s->brightness_slider)) { lv_obj_set_width(s->brightness_slider, LV_PCT(80)); lv_obj_align(s->brightness_slider, LV_ALIGN_BOTTOM_MID, 0, -15); lv_slider_set_range(s->brightness_slider, 10, 100); lv_slider_set_value(s->brightness_slider, g_ui_manager.settings.screen_brightness, LV_ANIM_OFF); lv_obj_add_event_cb(s->brightness_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)2); }

    /* 防夹功能 */
    s->antipinch_container = lv_obj_create(s->settings_list);
    if (!is_valid_obj(s->antipinch_container)) { destroy_settings_screen(); return -1; }
    lv_obj_set_size(s->antipinch_container, LV_PCT(100), 60);
    lv_obj_set_style_bg_color(s->antipinch_container, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(s->antipinch_container, 0, 0);
    s->antipinch_title = lv_label_create(s->antipinch_container);
    if (is_valid_obj(s->antipinch_title)) { lv_label_set_text(s->antipinch_title, "防夹功能"); lv_obj_align(s->antipinch_title, LV_ALIGN_LEFT_MID, 10, 0); lv_obj_set_style_text_color(s->antipinch_title, lv_color_hex(0xffffff), 0); }
    s->antipinch_switch = lv_switch_create(s->antipinch_container);
    if (is_valid_obj(s->antipinch_switch)) { lv_obj_align(s->antipinch_switch, LV_ALIGN_RIGHT_MID, -10, 0); if (g_ui_manager.settings.antipinch_enabled) lv_obj_add_state(s->antipinch_switch, LV_STATE_CHECKED); lv_obj_add_event_cb(s->antipinch_switch, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL); }

    g_ui_manager.screen_created[UI_SCREEN_SETTINGS] = SCREEN_CREATED;
    return 0;
}

static void destroy_settings_screen(void)
{
    if (is_valid_obj(g_ui_manager.settings_screen.screen)) { lv_obj_del(g_ui_manager.settings_screen.screen); }
    memset(&g_ui_manager.settings_screen, 0, sizeof(settings_screen_t));
    g_ui_manager.screen_created[UI_SCREEN_SETTINGS] = SCREEN_NOT_CREATED;
}

/* ==================== 内部界面切换 ==================== */

static int ui_switch_screen_internal(ui_screen_t screen)
{
    if (screen >= UI_SCREEN_MAX) return -1;

    int ret = 0;
    switch (screen) {
        case UI_SCREEN_MAIN: if (g_ui_manager.screen_created[UI_SCREEN_MAIN] != SCREEN_CREATED) ret = create_main_screen(); break;
        case UI_SCREEN_COUNTDOWN: if (g_ui_manager.screen_created[UI_SCREEN_COUNTDOWN] != SCREEN_CREATED) ret = create_countdown_screen(); break;
        case UI_SCREEN_ALERT: if (g_ui_manager.screen_created[UI_SCREEN_ALERT] != SCREEN_CREATED) ret = create_alarm_screen(); break;
        case UI_SCREEN_SETTINGS: if (g_ui_manager.screen_created[UI_SCREEN_SETTINGS] != SCREEN_CREATED) ret = create_settings_screen(); break;
        default: return -1;
    }
    if (ret < 0) return -1;

    lv_obj_t *scr = NULL;
    switch (screen) {
        case UI_SCREEN_MAIN: scr = g_ui_manager.main_screen.screen; break;
        case UI_SCREEN_COUNTDOWN: scr = g_ui_manager.countdown_screen.screen; break;
        case UI_SCREEN_ALERT: scr = g_ui_manager.alarm_screen.screen; break;
        case UI_SCREEN_SETTINGS: scr = g_ui_manager.settings_screen.screen; break;
        default: break;
    }

    if (is_valid_obj(scr)) {
        lv_scr_load(scr);
        g_ui_manager.current_screen = screen;
        UI_LOGI("Switched to screen: %d", screen);
        return 0;
    }
    return -1;
}

/* ==================== 公共 API ==================== */

int ui_manager_init(void)
{
    memset(&g_ui_manager, 0, sizeof(ui_manager_t));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_ui_manager.mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    pthread_cond_init(&g_ui_manager.callback_done, NULL);

    g_ui_manager.settings.auto_close_time = 10;
    g_ui_manager.settings.voice_volume = 80;
    g_ui_manager.settings.screen_brightness = 100;
    g_ui_manager.settings.antipinch_enabled = true;
    g_ui_manager.initialized = true;

    ui_lock();
    if (create_main_screen() < 0) { ui_unlock(); return -1; }
    if (is_valid_obj(g_ui_manager.main_screen.screen)) {
        lv_scr_load(g_ui_manager.main_screen.screen);
        g_ui_manager.current_screen = UI_SCREEN_MAIN;
    }
    g_ui_manager.time_update_timer = lv_timer_create(update_time_label_cb, 1000, NULL);
    ui_unlock();

    UI_LOGI("UI manager initialized");
    return 0;
}

/*
 * ui_manager_deinit() - 反初始化 UI 管理器
 *
 * 安全销毁 mutex 的条件：
 *   - 没有线程持有 mutex
 *   - 没有线程等待 mutex
 *
 * 使用条件变量等待回调执行完成：
 *   - 设置 deinitializing = true，阻止新的回调
 *   - 等待 callback_refcount == 0
 *   - 销毁 mutex
 */
int ui_manager_deinit(void)
{
    if (!g_ui_manager.initialized) return -1;

    /* 设置反初始化标志，阻止新的回调执行 */
    g_ui_manager.deinitializing = true;

    ui_lock();
    if (g_ui_manager.time_update_timer) { lv_timer_del(g_ui_manager.time_update_timer); g_ui_manager.time_update_timer = NULL; }
    if (g_ui_manager.countdown_timer) { lv_timer_del(g_ui_manager.countdown_timer); g_ui_manager.countdown_timer = NULL; }
    destroy_main_screen();
    destroy_countdown_screen();
    destroy_alarm_screen();
    destroy_settings_screen();
    g_ui_manager.initialized = false;
    g_ui_manager.pending_queue.head = 0;
    g_ui_manager.pending_queue.tail = 0;
    g_ui_manager.pending_queue.count = 0;
    g_ui_manager.event_callback = NULL;
    g_ui_manager.event_user_data = NULL;

    /* 等待所有回调执行完成（引用计数归零） */
    /* 使用条件变量，避免忙等待 */
    while (g_ui_manager.callback_refcount > 0) {
        UI_LOGW("Waiting for callbacks to complete (refcount=%d)", g_ui_manager.callback_refcount);
        /* pthread_cond_wait 会释放锁并等待信号 */
        /* 收到信号后重新获取锁，然后检查条件 */
        pthread_cond_wait(&g_ui_manager.callback_done, &g_ui_manager.mutex);
    }

    ui_unlock();

    /* 此时没有线程持有或等待 mutex，可以安全销毁 */
    pthread_cond_destroy(&g_ui_manager.callback_done);
    pthread_mutex_destroy(&g_ui_manager.mutex);
    UI_LOGI("UI manager deinitialized");
    return 0;
}

int ui_register_event_callback(ui_event_cb_t callback, void *user_data)
{
    if (!g_ui_manager.initialized) return -1;
    ui_lock();
    g_ui_manager.event_callback = callback;
    g_ui_manager.event_user_data = user_data;
    ui_unlock();
    return 0;
}

/*
 * ui_manager_process() - 主循环处理函数
 *
 * 锁契约（简单明确）：
 *   - 进入时：通过 trylock 获取锁（非阻塞，最多重试 3 次）
 *   - 返回时：锁被释放（无论是否发生 deinitializing）
 *
 * 引用计数保护 mutex 生命周期：
 *   - 释放锁前：callback_refcount++
 *   - 重新获取锁后：callback_refcount--
 *   - ui_manager_deinit() 等待 callback_refcount == 0 再销毁 mutex
 *
 * 设计取舍：
 *   - 使用 trylock 避免阻塞主循环
 *   - 重试 3 次平衡响应性和事件处理及时性
 *   - 如果 3 次都失败，返回 0，事件积压在队列下次处理
 */
int ui_manager_process(void)
{
    if (!g_ui_manager.initialized) return -1;

    /* 1. 尝试获取锁（非阻塞，最多重试 3 次） */
    int retry = 3;
    while (ui_trylock() != 0) {
        if (--retry <= 0) {
            return 0;  /* 3 次都失败，队列不消费，事件积压 */
        }
        usleep(100);  /* 100us，短暂让出 CPU */
    }

    /* 2. 复制待执行动作（在锁内） */
    pending_action_t actions[PENDING_ACTION_QUEUE_SIZE];
    int count = copy_pending_actions(actions, PENDING_ACTION_QUEUE_SIZE);

    if (count == 0) {
        ui_unlock();
        return 0;
    }

    /* 3. 执行界面切换（在锁内） */
    for (int i = 0; i < count; i++) {
        if (actions[i].switch_screen && actions[i].target_screen < UI_SCREEN_MAX) {
            ui_switch_screen_internal(actions[i].target_screen);
        }
    }

    /* 4. 检查是否正在反初始化 */
    if (g_ui_manager.deinitializing) {
        ui_unlock();  /* 释放锁 */
        return count; /* 不执行回调 */
    }

    /* 5. 增加引用计数，保护 mutex 生命周期 */
    g_ui_manager.callback_refcount++;

    /* 6. 保存回调指针 */
    ui_event_cb_t cb = g_ui_manager.event_callback;
    void *user_data = g_ui_manager.event_user_data;

    /* 7. 释放锁 */
    ui_unlock();

    /* 8. 执行回调（在锁外） */
    for (int i = 0; i < count; i++) {
        if (actions[i].event != UI_EVENT_NONE && cb) {
            UI_LOGD("Notifying event: %d", actions[i].event);
            cb(actions[i].event, actions[i].data, user_data);
        }
    }

    /* 9. 重新获取锁 */
    ui_lock();

    /* 10. 减少引用计数 */
    g_ui_manager.callback_refcount--;

    /* 11. 保存是否需要发送信号 */
    bool should_signal = (g_ui_manager.callback_refcount == 0);

    /* 12. 释放锁 */
    ui_unlock();

    /* 13. 在锁外发送信号（避免惊群效应） */
    if (should_signal) {
        pthread_cond_signal(&g_ui_manager.callback_done);
    }

    return count;
}

int ui_switch_screen(ui_screen_t screen)
{
    if (!g_ui_manager.initialized || screen >= UI_SCREEN_MAX) return -1;
    ui_lock();
    set_pending_action(UI_EVENT_NONE, NULL, screen, true);
    ui_unlock();
    return 0;
}

int ui_update_door_status(door_status_t status)
{
    if (!g_ui_manager.initialized) return -1;
    ui_lock();
    g_ui_manager.door_status = status;
    if (is_valid_obj(g_ui_manager.main_screen.door_label)) {
        const char *text[] = {"门: 关闭", "门: 打开", "门: 关闭中...", "门: 打开中..."};
        uint32_t color[] = {0x00ff00, 0xffd700, 0x00b4d8, 0x00b4d8};
        lv_label_set_text(g_ui_manager.main_screen.door_label, text[status]);
        lv_obj_set_style_text_color(g_ui_manager.main_screen.door_label, lv_color_hex(color[status]), 0);
    }
    ui_unlock();
    return 0;
}

int ui_update_lock_status(lock_status_t status)
{
    if (!g_ui_manager.initialized) return -1;
    ui_lock();
    g_ui_manager.lock_status = status;
    if (is_valid_obj(g_ui_manager.main_screen.lock_label) && is_valid_obj(g_ui_manager.main_screen.status_icon)) {
        if (status == LOCK_STATUS_LOCKED) {
            lv_label_set_text(g_ui_manager.main_screen.lock_label, "锁: 已锁定");
            lv_label_set_text(g_ui_manager.main_screen.status_icon, LV_SYMBOL_LOCK);
            lv_obj_set_style_text_color(g_ui_manager.main_screen.status_icon, lv_color_hex(0x00ff00), 0);
            lv_obj_set_style_text_color(g_ui_manager.main_screen.lock_label, lv_color_hex(0x00ff00), 0);
        } else {
            lv_label_set_text(g_ui_manager.main_screen.lock_label, "锁: 已解锁");
            lv_label_set_text(g_ui_manager.main_screen.status_icon, LV_SYMBOL_UNLOCK);
            lv_obj_set_style_text_color(g_ui_manager.main_screen.status_icon, lv_color_hex(0xffd700), 0);
            lv_obj_set_style_text_color(g_ui_manager.main_screen.lock_label, lv_color_hex(0xffd700), 0);
        }
    }
    ui_unlock();
    return 0;
}

int ui_update_system_status(const system_status_t *status)
{
    if (!g_ui_manager.initialized || !status) return -1;
    ui_lock();
    memcpy(&g_ui_manager.system_status, status, sizeof(system_status_t));

    if (is_valid_obj(g_ui_manager.main_screen.battery_icon)) {
        const char *icons[] = {LV_SYMBOL_BATTERY_EMPTY, LV_SYMBOL_BATTERY_1, LV_SYMBOL_BATTERY_2, LV_SYMBOL_BATTERY_3, LV_SYMBOL_BATTERY_FULL};
        uint32_t colors[] = {0xff0000, 0xff6b6b, 0xffd700, 0x00ff00, 0x00ff00};
        lv_label_set_text(g_ui_manager.main_screen.battery_icon, icons[status->battery_level]);
        lv_obj_set_style_text_color(g_ui_manager.main_screen.battery_icon, lv_color_hex(colors[status->battery_level]), 0);
    }

    if (is_valid_obj(g_ui_manager.main_screen.wifi_icon)) {
        lv_obj_set_style_text_color(g_ui_manager.main_screen.wifi_icon, lv_color_hex(status->wifi_connected ? 0x00ff00 : 0x666666), 0);
    }
    ui_unlock();
    return 0;
}

int ui_show_countdown(int seconds)
{
    if (!g_ui_manager.initialized || seconds <= 0 || seconds > 30) return -1;

    ui_lock();
    g_ui_manager.countdown_seconds = seconds;
    g_ui_manager.countdown_total = seconds;

    /* 统一走 pending 队列 */
    set_pending_action(UI_EVENT_NONE, NULL, UI_SCREEN_COUNTDOWN, true);

    if (is_valid_obj(g_ui_manager.countdown_screen.countdown_label)) {
        lv_label_set_text_fmt(g_ui_manager.countdown_screen.countdown_label, "%d秒后自动关门", seconds);
    }
    if (is_valid_obj(g_ui_manager.countdown_screen.countdown_bar)) {
        lv_bar_set_range(g_ui_manager.countdown_screen.countdown_bar, 0, seconds);
        lv_bar_set_value(g_ui_manager.countdown_screen.countdown_bar, seconds, LV_ANIM_OFF);
    }

    if (g_ui_manager.countdown_timer) { lv_timer_del(g_ui_manager.countdown_timer); }
    g_ui_manager.countdown_timer = lv_timer_create(countdown_timer_cb, 1000, NULL);

    ui_unlock();
    UI_LOGI("Countdown started: %ds", seconds);
    return 0;
}

int ui_cancel_countdown(void)
{
    if (!g_ui_manager.initialized) return -1;
    ui_lock();
    if (g_ui_manager.countdown_timer) { lv_timer_del(g_ui_manager.countdown_timer); g_ui_manager.countdown_timer = NULL; }
    /* 统一走 pending 队列 */
    set_pending_action(UI_EVENT_COUNTDOWN_CANCEL, NULL, UI_SCREEN_MAIN, true);
    ui_unlock();
    return 0;
}

int ui_show_alarm(alarm_type_t type)
{
    if (!g_ui_manager.initialized) return -1;

    ui_lock();

    /* 修复：先创建报警界面，再设置控件 */
    /* 调用内部函数创建界面（如果未创建） */
    if (g_ui_manager.screen_created[UI_SCREEN_ALERT] != SCREEN_CREATED) {
        if (create_alarm_screen() < 0) {
            UI_LOGE("Failed to create alarm screen");
            ui_unlock();
            return -1;
        }
    }

    /* 设置报警内容 */
    const char *titles[] = {"防夹报警！", "异常开门！", "电量不足！", "防撬报警！"};
    const char *details[] = {"检测到门体夹到物体，请检查", "门被异常打开，请检查", "电池电量低，请及时更换", "检测到门锁被撬动"};

    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_label)) {
        lv_label_set_text(g_ui_manager.alarm_screen.alarm_label, titles[type]);
    }
    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_detail)) {
        lv_label_set_text(g_ui_manager.alarm_screen.alarm_detail, details[type]);
    }

    /* 创建闪烁动画 */
    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_icon)) {
        lv_anim_del(g_ui_manager.alarm_screen.alarm_icon, NULL);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, g_ui_manager.alarm_screen.alarm_icon);
        lv_anim_set_values(&a, 0, 255);
        lv_anim_set_time(&a, 500);
        lv_anim_set_playback_time(&a, 500);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_start(&a);
        g_ui_manager.alarm_screen.anim_active = true;
    }

    /* 统一走 pending 队列切换界面 */
    set_pending_action(UI_EVENT_NONE, NULL, UI_SCREEN_ALERT, true);

    ui_unlock();
    return 0;
}

int ui_close_alarm(void)
{
    if (!g_ui_manager.initialized) return -1;
    ui_lock();
    if (is_valid_obj(g_ui_manager.alarm_screen.alarm_icon)) lv_anim_del(g_ui_manager.alarm_screen.alarm_icon, NULL);
    g_ui_manager.alarm_screen.anim_active = false;
    /* 统一走 pending 队列 */
    set_pending_action(UI_EVENT_ALARM_CONFIRM, NULL, UI_SCREEN_MAIN, true);
    ui_unlock();
    return 0;
}

int ui_get_settings(ui_settings_t *settings)
{
    if (!g_ui_manager.initialized || !settings) return -1;
    ui_lock();
    memcpy(settings, &g_ui_manager.settings, sizeof(ui_settings_t));
    ui_unlock();
    return 0;
}

int ui_set_settings(const ui_settings_t *settings)
{
    if (!g_ui_manager.initialized || !settings) return -1;
    ui_lock();
    memcpy(&g_ui_manager.settings, settings, sizeof(ui_settings_t));
    if (g_ui_manager.screen_created[UI_SCREEN_SETTINGS] == SCREEN_CREATED) {
        if (is_valid_obj(g_ui_manager.settings_screen.close_time_slider)) lv_slider_set_value(g_ui_manager.settings_screen.close_time_slider, settings->auto_close_time, LV_ANIM_OFF);
        if (is_valid_obj(g_ui_manager.settings_screen.volume_slider)) lv_slider_set_value(g_ui_manager.settings_screen.volume_slider, settings->voice_volume, LV_ANIM_OFF);
        if (is_valid_obj(g_ui_manager.settings_screen.brightness_slider)) lv_slider_set_value(g_ui_manager.settings_screen.brightness_slider, settings->screen_brightness, LV_ANIM_OFF);
        if (is_valid_obj(g_ui_manager.settings_screen.antipinch_switch)) {
            if (settings->antipinch_enabled) lv_obj_add_state(g_ui_manager.settings_screen.antipinch_switch, LV_STATE_CHECKED);
            else lv_obj_clear_state(g_ui_manager.settings_screen.antipinch_switch, LV_STATE_CHECKED);
        }
    }
    ui_unlock();
    return 0;
}

ui_screen_t ui_get_current_screen(void) { return g_ui_manager.current_screen; }

bool ui_is_screen_created(ui_screen_t screen)
{
    return (screen < UI_SCREEN_MAX) && (g_ui_manager.screen_created[screen] == SCREEN_CREATED);
}
