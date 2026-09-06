/**
 * @file main.c
 * @brief 智能锁应用主程序
 * @version 1.0.0
 * @date 2024-01-01
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#include "ui_manager.h"

/**
 * @brief 模拟门传感器检测
 */
static void simulate_door_sensor(void)
{
    static int count = 0;
    count++;

    /* 模拟门状态变化 */
    if (count % 10 == 0) {
        ui_manager_update_door_status(DOOR_STATUS_OPEN);
        syslog(LOG_INFO, "Door opened\n");

        /* 显示倒计时界面 */
        ui_manager_show_countdown(ui_manager_get_auto_close_time());
    } else if (count % 10 == 5) {
        ui_manager_update_door_status(DOOR_STATUS_CLOSED);
        syslog(LOG_INFO, "Door closed\n");
    }
}

/**
 * @brief 模拟锁状态变化
 */
static void simulate_lock_status(void)
{
    static int count = 0;
    count++;

    /* 模拟锁状态变化 */
    if (count % 15 == 0) {
        ui_manager_update_lock_status(LOCK_STATUS_UNLOCKED);
        syslog(LOG_INFO, "Lock unlocked\n");
    } else if (count % 15 == 7) {
        ui_manager_update_lock_status(LOCK_STATUS_LOCKED);
        syslog(LOG_INFO, "Lock locked\n");
    }
}

/**
 * @brief 模拟系统状态更新
 */
static void simulate_system_status(void)
{
    system_status_t status;

    /* 模拟电池电量变化 */
    static int battery_count = 0;
    battery_count++;
    if (battery_count % 20 == 0) {
        status.battery_level = BATTERY_LEVEL_FULL;
    } else if (battery_count % 20 == 5) {
        status.battery_level = BATTERY_LEVEL_3;
    } else if (battery_count % 20 == 10) {
        status.battery_level = BATTERY_LEVEL_2;
    } else if (battery_count % 20 == 15) {
        status.battery_level = BATTERY_LEVEL_1;
    }

    /* 模拟 WiFi 连接状态 */
    status.wifi_connected = (battery_count % 30 < 20);

    /* 更新系统状态 */
    ui_manager_update_system_status(status);
}

/**
 * @brief 模拟报警事件
 */
static void simulate_alarm_event(void)
{
    static int count = 0;
    count++;

    /* 模拟报警事件 */
    if (count % 25 == 0) {
        ui_manager_show_alarm(ALARM_ANTI_PINCH);
        syslog(LOG_INFO, "Antipinch alarm triggered\n");
    } else if (count % 25 == 12) {
        ui_manager_show_alarm(ALARM_ABNORMAL_OPEN);
        syslog(LOG_INFO, "Abnormal open alarm triggered\n");
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
    int ret;

    syslog(LOG_INFO, "Smart Lock application starting...\n");

    /* 初始化 UI 管理器 */
    ret = ui_manager_init();
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to initialize UI manager: %d\n", ret);
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Smart Lock application initialized successfully\n");

    /* 主循环 */
    while (1) {
        /* 模拟传感器检测 */
        simulate_door_sensor();
        simulate_lock_status();
        simulate_system_status();
        simulate_alarm_event();

        /* 延时 1 秒 */
        sleep(1);
    }

    return EXIT_SUCCESS;
}
