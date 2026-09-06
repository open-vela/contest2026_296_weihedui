/**
 * @file ui_manager.h
 * @brief 智能锁 LVGL 界面管理器头文件
 * @version 2.3.0
 * @date 2024-01-01
 */

#ifndef __UI_MANAGER_H
#define __UI_MANAGER_H

#include <nuttx/config.h>
#include <stdbool.h>
#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 界面 ID 定义
 */
typedef enum {
    UI_SCREEN_MAIN = 0,      /* 主界面 */
    UI_SCREEN_COUNTDOWN,     /* 倒计时界面 */
    UI_SCREEN_ALERT,         /* 报警界面 */
    UI_SCREEN_SETTINGS,      /* 设置界面 */
    UI_SCREEN_MAX
} ui_screen_t;

/**
 * @brief 门状态枚举
 */
typedef enum {
    DOOR_STATUS_CLOSED = 0,  /* 关闭 */
    DOOR_STATUS_OPEN,        /* 打开 */
    DOOR_STATUS_CLOSING,     /* 关闭中 */
    DOOR_STATUS_OPENING      /* 打开中 */
} door_status_t;

/**
 * @brief 锁状态枚举
 */
typedef enum {
    LOCK_STATUS_LOCKED = 0,  /* 已锁定 */
    LOCK_STATUS_UNLOCKED     /* 已解锁 */
} lock_status_t;

/**
 * @brief 电池电量枚举
 */
typedef enum {
    BATTERY_LEVEL_EMPTY = 0, /* 空 */
    BATTERY_LEVEL_1,         /* 1格 */
    BATTERY_LEVEL_2,         /* 2格 */
    BATTERY_LEVEL_3,         /* 3格 */
    BATTERY_LEVEL_FULL       /* 满电 */
} battery_level_t;

/**
 * @brief 报警类型枚举
 */
typedef enum {
    ALARM_ANTI_PINCH = 0,    /* 防夹报警 */
    ALARM_ABNORMAL_OPEN,     /* 异常开门报警 */
    ALARM_LOW_BATTERY,       /* 低电量报警 */
    ALARM_TAMPER             /* 防撬报警 */
} alarm_type_t;

/**
 * @brief UI 事件类型枚举
 */
typedef enum {
    UI_EVENT_NONE = 0,       /* 无事件 */
    UI_EVENT_UNLOCK,         /* 开锁事件 */
    UI_EVENT_LOCK,           /* 锁定事件 */
    UI_EVENT_COUNTDOWN_END,  /* 倒计时结束事件 */
    UI_EVENT_COUNTDOWN_CANCEL, /* 取消倒计时事件 */
    UI_EVENT_ALARM_CONFIRM,  /* 确认报警事件 */
    UI_EVENT_SETTINGS_CHANGED, /* 设置变更事件 */
    UI_EVENT_MAX
} ui_event_t;

/**
 * @brief 系统状态结构体
 */
typedef struct {
    battery_level_t battery_level;  /* 电池电量 */
    bool wifi_connected;            /* WiFi 连接状态 */
    bool charging;                  /* 充电状态 */
    int signal_strength;            /* 信号强度 */
} system_status_t;

/**
 * @brief 设置参数结构体
 */
typedef struct {
    int auto_close_time;      /* 自动关门时间 (5-30秒) */
    int voice_volume;         /* 语音音量 (0-100) */
    int screen_brightness;    /* 屏幕亮度 (0-100) */
    bool antipinch_enabled;   /* 防夹功能开关 */
} ui_settings_t;

/**
 * @brief UI 事件回调函数类型
 * @param event 事件类型
 * @param data 事件数据
 * @param user_data 用户数据
 */
typedef void (*ui_event_cb_t)(ui_event_t event, void *data, void *user_data);

/**
 * @brief 初始化 UI 管理器
 * @return 0: 成功, 其他: 失败
 */
int ui_manager_init(void);

/**
 * @brief 反初始化 UI 管理器
 * @return 0: 成功, 其他: 失败
 */
int ui_manager_deinit(void);

/**
 * @brief 注册 UI 事件回调
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0: 成功, 其他: 失败
 */
int ui_register_event_callback(ui_event_cb_t callback, void *user_data);

/**
 * @brief 主循环处理函数（需要在主循环中定期调用）
 *
 * 处理待执行的界面切换动作，避免在定时器回调中直接切换界面导致的跨锁风险。
 * 建议在主循环中每 10ms 调用一次。
 *
 * @return 0: 成功, 其他: 失败
 */
int ui_manager_process(void);

/**
 * @brief 切换界面
 * @param screen 界面 ID
 * @return 0: 成功, 其他: 失败
 */
int ui_switch_screen(ui_screen_t screen);

/**
 * @brief 更新门状态
 * @param status 门状态
 * @return 0: 成功, 其他: 失败
 */
int ui_update_door_status(door_status_t status);

/**
 * @brief 更新锁状态
 * @param status 锁状态
 * @return 0: 成功, 其他: 失败
 */
int ui_update_lock_status(lock_status_t status);

/**
 * @brief 更新系统状态
 * @param status 系统状态
 * @return 0: 成功, 其他: 失败
 */
int ui_update_system_status(const system_status_t *status);

/**
 * @brief 显示倒计时界面
 * @param seconds 倒计时秒数
 * @return 0: 成功, 其他: 失败
 */
int ui_show_countdown(int seconds);

/**
 * @brief 取消倒计时
 * @return 0: 成功, 其他: 失败
 */
int ui_cancel_countdown(void);

/**
 * @brief 显示报警界面
 * @param type 报警类型
 * @return 0: 成功, 其他: 失败
 */
int ui_show_alarm(alarm_type_t type);

/**
 * @brief 关闭报警界面
 * @return 0: 成功, 其他: 失败
 */
int ui_close_alarm(void);

/**
 * @brief 获取设置参数
 * @param settings 设置参数指针
 * @return 0: 成功, 其他: 失败
 */
int ui_get_settings(ui_settings_t *settings);

/**
 * @brief 设置参数
 * @param settings 设置参数指针
 * @return 0: 成功, 其他: 失败
 */
int ui_set_settings(const ui_settings_t *settings);

/**
 * @brief 获取当前界面 ID
 * @return 界面 ID
 */
ui_screen_t ui_get_current_screen(void);

/**
 * @brief 检查界面是否已创建
 * @param screen 界面 ID
 * @return true: 已创建, false: 未创建
 */
bool ui_is_screen_created(ui_screen_t screen);

#ifdef __cplusplus
}
#endif

#endif /* __UI_MANAGER_H */
