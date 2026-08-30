/**
 * @file safety.h
 * @brief 安全模块头文件
 *
 * 本文件定义了防夹保护功能的接口和数据结构。
 *
 * Copyright (C) 2026 weihedui Team
 * Licensed under the Apache License, Version 2.0
 */

#ifndef __SAFETY_H__
#define __SAFETY_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 雷达状态定义
 */
typedef enum {
    RADAR_STATE_NO_TARGET,    /**< 无目标 */
    RADAR_STATE_MOVING,       /**< 目标移动中 */
    RADAR_STATE_STATIONARY    /**< 目标静止 */
} radar_state_t;

/**
 * @brief 雷达数据结构体
 */
typedef struct {
    radar_state_t state;      /**< 雷达状态 */
    uint16_t distance_cm;     /**< 距离（厘米） */
    uint16_t energy;          /**< 能量值 */
    uint32_t timestamp;       /**< 时间戳 */
} radar_result_t;

/**
 * @brief 安全配置结构体
 */
typedef struct {
    uint8_t anti_pinch_threshold;  /**< 防夹检测阈值（连续检测次数） */
    uint32_t check_interval_ms;    /**< 检测间隔（毫秒） */
    uint16_t detection_range_cm;   /**< 检测范围（厘米） */
} safety_config_t;

/**
 * @brief 初始化安全模块
 *
 * @param config 安全配置参数
 * @return int 0: 成功, -1: 失败
 */
int safety_init(const safety_config_t *config);

/**
 * @brief 反初始化安全模块
 *
 * @return int 0: 成功, -1: 失败
 */
int safety_deinit(void);

/**
 * @brief 启动防夹检测
 *
 * @return int 0: 成功, -1: 失败
 */
int safety_start(void);

/**
 * @brief 停止防夹检测
 *
 * @return int 0: 成功, -1: 失败
 */
int safety_stop(void);

/**
 * @brief 获取雷达状态
 *
 * @param result 雷达数据输出
 * @return int 0: 成功, -1: 失败
 */
int radar_get_status(radar_result_t *result);

/**
 * @brief 检查是否触发防夹保护
 *
 * @return true: 触发防夹保护, false: 未触发
 */
bool safety_is_anti_pinch_triggered(void);

/**
 * @brief 重置防夹检测状态
 */
void safety_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __SAFETY_H__ */
