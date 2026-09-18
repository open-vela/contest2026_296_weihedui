/**
 * @file safety.c
 * @brief 安全模块实现
 *
 * 本文件实现了门锁的防夹保护功能，包括：
 * - 雷达数据实时监测
 * - 防夹判断逻辑（连续3次检测到目标触发保护）
 * - 紧急响应（停止电机、反向开门、蜂鸣器报警）
 * - 状态恢复机制
 *
 * Copyright (C) 2026 weihedui Team
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "safety.h"
#include "door_control.h"
#include "radar_driver.h"   /* 集成修复：防夹逻辑改为读取真实雷达驱动 */

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SAFETY_TASK_STACK_SIZE  4096
#define DEFAULT_CHECK_INTERVAL  20   /* 20ms */
#define DEFAULT_THRESHOLD       3    /* 连续3次 */
#define DEFAULT_RANGE_CM        100  /* 100cm */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/**
 * @brief 安全模块上下文结构体
 */
typedef struct {
    bool initialized;              /**< 是否已初始化 */
    bool running;                  /**< 检测任务是否运行 */
    bool anti_pinch_triggered;     /**< 防夹是否触发 */
    safety_config_t config;        /**< 安全配置 */
    uint8_t detect_count;          /**< 连续检测计数 */
    pthread_t safety_thread;       /**< 安全检测线程 */
    pthread_mutex_t mutex;         /**< 互斥锁 */
} safety_context_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void *safety_task(void *arg);
static int radar_read_data(radar_result_t *result);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static safety_context_t g_safety_ctx = {
    .initialized = false,
    .running = false,
    .anti_pinch_triggered = false,
    .detect_count = 0,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 读取雷达数据（模拟实现）
 *
 * @param result 雷达数据输出
 * @return int 0: 成功, -1: 失败
 */
static int radar_read_data(radar_result_t *result)
{
    if (result == NULL) {
        return -1;
    }

    /* 集成修复 (2026-09-18)
     *
     * 原实现此处写着 "TODO: 替换为实际的雷达驱动调用"，并用
     * call_count % 100 < 10 伪造雷达数据。后果是防夹保护、主循环的自动
     * 关门判定、以及 agent_tools.c 上报给 AI Agent 的雷达状态，全部建立
     * 在假数据之上 —— 防夹保护实际上永远不会对真实障碍起作用。
     *
     * 现改为调用底层驱动 radar_driver_get_status()，并做字段映射：
     *   驱动层 radar_driver_result_t   ->   应用层 radar_result_t
     *     target_detected / motion_state  ->   state
     *     target_distance                 ->   distance
     *     energy_value                    ->   energy
     *     timestamp                       ->   timestamp
     */

    radar_driver_result_t raw;

    if (radar_driver_get_status(&raw) < 0) {
        return -1;
    }

    if (!raw.target_detected) {
        result->state = RADAR_STATE_NO_TARGET;
    } else if (raw.motion_state == RADAR_MOTION_MOVING) {
        result->state = RADAR_STATE_MOVING;
    } else if (raw.motion_state == RADAR_MOTION_STATIONARY) {
        result->state = RADAR_STATE_STATIONARY;
    } else {
        result->state = RADAR_STATE_NO_TARGET;
    }

    result->distance  = raw.target_distance;
    result->energy    = raw.energy_value;
    result->timestamp = raw.timestamp ? raw.timestamp : (uint32_t)time(NULL);

    /* HLK-LD2410 是单区域雷达，驱动结果里没有内外侧信息；内外侧判定
     * 需要第二路雷达或 region 帧支持，这里暂固定为室内侧。 */

    result->zone = RADAR_ZONE_INDOOR;

    return 0;
}

/**
 * @brief 安全检测任务
 *
 * @param arg 任务参数（未使用）
 * @return void* 返回值（未使用）
 */
static void *safety_task(void *arg)
{
    (void)arg;

    radar_result_t radar;

    printf("[SAFETY] Detection task started\n");

    while (g_safety_ctx.running) {
        usleep(g_safety_ctx.config.check_interval_ms * 1000);

        /* 仅在关门过程中检测 */
        motor_state_t motor_state = motor_get_state();
        if (motor_state != MOTOR_STATE_CLOSING) {
            /* 不在关门状态，重置计数 */
            pthread_mutex_lock(&g_safety_ctx.mutex);
            g_safety_ctx.detect_count = 0;
            pthread_mutex_unlock(&g_safety_ctx.mutex);
            continue;
        }

        /* 读取雷达数据 */
        if (radar_read_data(&radar) != 0) {
            printf("[SAFETY] Failed to read radar data\n");
            continue;
        }

        pthread_mutex_lock(&g_safety_ctx.mutex);

        /* 检测到目标 */
        if (radar.state == RADAR_STATE_MOVING ||
            radar.state == RADAR_STATE_STATIONARY) {

            /* 检查是否在检测范围内 */
            if (radar.distance <= g_safety_ctx.config.detection_range_cm) {
                g_safety_ctx.detect_count++;

                printf("[SAFETY] Target detected! Count: %d/%d, Distance: %d cm\n",
                       g_safety_ctx.detect_count,
                       g_safety_ctx.config.anti_pinch_threshold,
                       radar.distance);

                /* 连续检测次数达到阈值 */
                if (g_safety_ctx.detect_count >= g_safety_ctx.config.anti_pinch_threshold) {
                    printf("[SAFETY] *** ANTI-PINCH TRIGGERED! ***\n");

                    /* 标记防夹已触发 */
                    g_safety_ctx.anti_pinch_triggered = true;

                    /* 紧急停止电机 */
                    motor_control(MOTOR_CMD_EMERGENCY_STOP);

                    /* 反向开门 */
                    usleep(100000); /* 等待100ms */
                    motor_control(MOTOR_CMD_OPEN);

                    /* 触发蜂鸣器报警 */
                    buzzer_alarm(500);

                    /* 等待目标离开 */
                    printf("[SAFETY] Waiting for target to leave...\n");
                    while (g_safety_ctx.running) {
                        usleep(100000); /* 100ms */
                        if (radar_read_data(&radar) == 0) {
                            if (radar.state == RADAR_STATE_NO_TARGET) {
                                printf("[SAFETY] Target left, resuming operation\n");
                                break;
                            }
                        }
                    }

                    /* 重置计数 */
                    g_safety_ctx.detect_count = 0;
                    g_safety_ctx.anti_pinch_triggered = false;
                }
            } else {
                /* 超出检测范围，重置计数 */
                g_safety_ctx.detect_count = 0;
            }
        } else {
            /* 未检测到目标，重置计数 */
            g_safety_ctx.detect_count = 0;
        }

        pthread_mutex_unlock(&g_safety_ctx.mutex);
    }

    printf("[SAFETY] Detection task stopped\n");

    return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int safety_init(const safety_config_t *config)
{
    if (g_safety_ctx.initialized) {
        printf("[SAFETY] Warning: already initialized\n");
        return 0;
    }

    /* 使用默认配置或用户配置 */
    if (config != NULL) {
        memcpy(&g_safety_ctx.config, config, sizeof(safety_config_t));
    } else {
        g_safety_ctx.config.anti_pinch_threshold = DEFAULT_THRESHOLD;
        g_safety_ctx.config.check_interval_ms = DEFAULT_CHECK_INTERVAL;
        g_safety_ctx.config.detection_range_cm = DEFAULT_RANGE_CM;
    }

    /* 初始化互斥锁 */
    pthread_mutex_init(&g_safety_ctx.mutex, NULL);

    /* 初始化状态 */
    g_safety_ctx.detect_count = 0;
    g_safety_ctx.anti_pinch_triggered = false;
    g_safety_ctx.running = false;
    g_safety_ctx.initialized = true;

    printf("[SAFETY] Initialized successfully\n");
    printf("[SAFETY]   Threshold: %d\n", g_safety_ctx.config.anti_pinch_threshold);
    printf("[SAFETY]   Check interval: %u ms\n", g_safety_ctx.config.check_interval_ms);
    printf("[SAFETY]   Detection range: %u cm\n", g_safety_ctx.config.detection_range_cm);

    return 0;
}

int safety_deinit(void)
{
    if (!g_safety_ctx.initialized) {
        return 0;
    }

    /* 停止检测任务 */
    safety_stop();

    /* 销毁互斥锁 */
    pthread_mutex_destroy(&g_safety_ctx.mutex);

    g_safety_ctx.initialized = false;

    printf("[SAFETY] Deinitialized\n");

    return 0;
}

int safety_start(void)
{
    if (!g_safety_ctx.initialized) {
        printf("[SAFETY] Error: not initialized\n");
        return -1;
    }

    if (g_safety_ctx.running) {
        printf("[SAFETY] Warning: already running\n");
        return 0;
    }

    g_safety_ctx.running = true;

    int ret = pthread_create(&g_safety_ctx.safety_thread, NULL,
                             safety_task, NULL);
    if (ret != 0) {
        printf("[SAFETY] Failed to create safety thread: %d\n", ret);
        g_safety_ctx.running = false;
        return -1;
    }

    printf("[SAFETY] Detection started\n");

    return 0;
}

int safety_stop(void)
{
    if (!g_safety_ctx.running) {
        return 0;
    }

    g_safety_ctx.running = false;
    pthread_join(g_safety_ctx.safety_thread, NULL);

    printf("[SAFETY] Detection stopped\n");

    return 0;
}

int radar_get_status(radar_result_t *result)
{
    if (result == NULL) {
        return -1;
    }

    return radar_read_data(result);
}

bool safety_is_anti_pinch_triggered(void)
{
    return g_safety_ctx.anti_pinch_triggered;
}

void safety_reset(void)
{
    pthread_mutex_lock(&g_safety_ctx.mutex);

    g_safety_ctx.detect_count = 0;
    g_safety_ctx.anti_pinch_triggered = false;

    pthread_mutex_unlock(&g_safety_ctx.mutex);

    printf("[SAFETY] Reset\n");
}
