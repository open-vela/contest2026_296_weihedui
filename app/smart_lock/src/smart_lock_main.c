/****************************************************************************
 * app/smart_lock/src/smart_lock_main.c
 *
 * 智锁卫士 - 应用入口与集成层
 *
 * 本文件此前缺失，导致：
 *   - 没有任何 main()/入口点，应用无法被启动；
 *   - g_default_motor_config / g_default_safety_config 两个配置实例
 *     无人定义，door_control.c 与 safety.c 都无法链接；
 *   - agent_tools.c 调用的 set_auto_close_timeout() 全仓库无实现。
 *
 * 因此这里集中提供三样东西：
 *   1. 两个默认配置实例；
 *   2. set_auto_close_timeout() 的实现，以及主循环里的自动关门判定；
 *   3. 各模块的初始化顺序与应用入口。
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <nuttx/config.h>

#include "smart_lock.h"
#include "display.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 自动关门超时：优先取 Kconfig，未配置时退化为 10 秒 */
#ifdef CONFIG_SMART_LOCK_AUTO_LOCK_TIMEOUT
#  define AUTO_CLOSE_TIMEOUT_DEFAULT CONFIG_SMART_LOCK_AUTO_LOCK_TIMEOUT
#else
#  define AUTO_CLOSE_TIMEOUT_DEFAULT 10
#endif

/* 主循环节拍 */
#define MAIN_LOOP_INTERVAL_S 1

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 电机默认配置
 *
 * 注意：四个 GPIO 引脚号目前均为占位值 0。真实硬件接线确定后必须改为
 * 黄山派上实际使用的引脚号，否则电机与蜂鸣器不会动作。
 */

const motor_config_t g_default_motor_config = {
    .open_gpio_pin    = 0,
    .close_gpio_pin   = 0,
    .lock_gpio_pin    = 0,
    .buzzer_gpio_pin  = 0,
    .open_timeout_ms  = 3000,
    .close_timeout_ms = 3000,
    .lock_timeout_ms  = 2000,
};

/* 防夹保护默认配置 */

const safety_config_t g_default_safety_config = {
    .anti_pinch_threshold = 3,    /* 连续 3 次检测到障碍即触发 */
    .check_interval_ms    = 100,  /* 每 100ms 检测一次 */
    .detection_range_cm   = 50,   /* 50cm 内视为有障碍 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_auto_close_timeout_s = AUTO_CLOSE_TIMEOUT_DEFAULT;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * @brief 设置自动关门超时时间
 *
 * agent_tools.c 的 tool_set_timeout() 会调用本函数把 AI Agent 下发的
 * 超时值写进来，主循环据此判断是否该自动关门。
 *
 * @param seconds 超时秒数，有效范围 1-300
 * @return int 0: 成功, -EINVAL: 参数非法
 */

int set_auto_close_timeout(int seconds)
{
    if (seconds <= 0 || seconds > 300)
    {
        return -EINVAL;
    }

    g_auto_close_timeout_s = seconds;
    printf("[MAIN] Auto close timeout set to %d s\n", seconds);
    return 0;
}

/**
 * @brief 读取当前自动关门超时时间
 *
 * @return int 当前超时秒数
 */

int get_auto_close_timeout(void)
{
    return g_auto_close_timeout_s;
}

/**
 * @brief 智锁卫士应用入口
 *
 * @param argc 参数个数
 * @param argv 参数列表
 * @return int 0: 正常退出
 */

int smart_lock_main(int argc, char *argv[])
{
    int ret;
    time_t door_open_since = 0;

    printf("[MAIN] ============================================\n");
    printf("[MAIN] Smart Lock Guardian starting (Team 296)\n");
    printf("[MAIN] auto close timeout = %d s\n", g_auto_close_timeout_s);
    printf("[MAIN] ============================================\n");

    /* 1. 雷达驱动 */

    ret = radar_init();
    if (ret < 0)
    {
        printf("[MAIN] radar_init failed: %d\n", ret);
    }

    /* 2. 门磁传感器 */

    ret = door_sensor_init();
    if (ret < 0)
    {
        printf("[MAIN] door_sensor_init failed: %d\n", ret);
    }

    /* 3. 电机与蜂鸣器控制 */

    ret = door_control_init(&g_default_motor_config);
    if (ret < 0)
    {
        printf("[MAIN] door_control_init failed: %d\n", ret);
    }

    /* 4. 防夹安全模块 */

    ret = safety_init(&g_default_safety_config);
    if (ret < 0)
    {
        printf("[MAIN] safety_init failed: %d\n", ret);
    }

    ret = safety_start();
    if (ret < 0)
    {
        printf("[MAIN] safety_start failed: %d\n", ret);
    }

    /* 5. BLE 服务 */

    ret = ble_service_init();
    if (ret < 0)
    {
        printf("[MAIN] ble_service_init failed: %d\n", ret);
    }

    ret = ble_start_advertising();
    if (ret < 0)
    {
        printf("[MAIN] ble_start_advertising failed: %d\n", ret);
    }

    /* 6. AMOLED 状态显示（纯附加，失败不影响门锁功能） */

    ret = display_init();
    if (ret < 0)
    {
        printf("[MAIN] display_init failed: %d (running headless)\n", ret);
    }

    /* 7. AMOLED 亮度通路自检
     *
     * 同样是附加功能：失败不影响门锁逻辑。这里只是把自检结果打进启动
     * 日志（[PBRT] 前缀），产测时一眼就能看出亮度寄存器是否可用；
     * 真正的调光由 `agent brightness` 命令触发。
     */

    ret = panel_brightness_init();
    if (ret < 0)
    {
        printf("[MAIN] panel_brightness_init failed: %d\n", ret);
    }

    printf("[MAIN] init done, entering main loop\n");

    /* 主循环 */

    for (;;)
    {
        door_status_t  door;
        radar_result_t radar;
        bool occupied = false;

        /* 5.1 跟踪门开启时长 */

        if (door_sensor_get_status(&door) == 0)
        {
            if (door.door_state == DOOR_STATE_OPEN)
            {
                if (door_open_since == 0)
                {
                    door_open_since = time(NULL);
                    printf("[MAIN] Door opened, auto close timer armed\n");
                }
            }
            else
            {
                door_open_since = 0;
            }
        }

        /* 5.2 判断检测范围内是否有人
         *
         * 距离为 0 视为无效读数，不计入"有人"，否则会因雷达驱动未接
         * 而永远判为有人、自动关门永不触发。
         */

        if (radar_get_status(&radar) == 0 &&
            radar.state != RADAR_STATE_NO_TARGET &&
            radar.distance > 0 &&
            radar.distance <= g_default_safety_config.detection_range_cm)
        {
            occupied = true;
        }

        /* 5.3 自动关门 */

        if (door_open_since != 0 && !occupied && g_auto_close_timeout_s > 0 &&
            (time(NULL) - door_open_since) >= g_auto_close_timeout_s)
        {
            printf("[MAIN] Auto close: door open for %d s without target\n",
                   g_auto_close_timeout_s);

            motor_control(MOTOR_CMD_CLOSE);
            door_open_since = 0;
        }

        /* 5.4 防夹保护 */

        if (safety_is_anti_pinch_triggered())
        {
            printf("[MAIN] Anti-pinch triggered, emergency stop\n");
            motor_control(MOTOR_CMD_EMERGENCY_STOP);
            safety_reset();
        }

        sleep(MAIN_LOOP_INTERVAL_S);
    }

    return 0;
}

/**
 * @brief NuttX 应用入口
 *
 * nuttx_add_application() 以 MAINSRC 方式注册时需要一个 main()，
 * 这里直接转发到 smart_lock_main()。
 */

int main(int argc, char *argv[])
{
    return smart_lock_main(argc, argv);
}
