/**
 * @file safety.h
 * @brief 安全模块头文件（应用级雷达视图 + 防夹保护）
 *
 * 本文件定义了防夹保护功能的接口和数据结构。
 *
 * 集成修复说明（2026-09-18）：
 *   原版本在此处重复定义了 radar_result_t / radar_state_t，与
 *   radar_driver.h 中的同名类型字段布局完全不同，两个头文件一旦被同一
 *   个翻译单元包含即编译失败。现明确划分为两层：
 *
 *     radar_driver.h  —— 底层驱动原始读数  radar_driver_result_t
 *     safety.h        —— 应用级雷达视图    radar_result_t（本文件）
 *
 *   同时补上 agent_tools.c 依赖的 zone / distance 字段。
 *   另外原文件末尾重复声明了 radar_get_status()，与 radar_driver.c 中的
 *   全局符号冲突；底层驱动的对应函数已更名为 radar_driver_get_status()。
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
 * @brief 雷达区域定义（应用级）
 */
typedef enum {
    RADAR_ZONE_INDOOR = 0,    /**< 室内侧 */
    RADAR_ZONE_OUTDOOR        /**< 室外侧 */
} radar_zone_t;

/**
 * @brief 雷达状态定义（应用级）
 */
typedef enum {
    RADAR_STATE_NO_TARGET = 0, /**< 无目标 */
    RADAR_STATE_MOVING,        /**< 目标移动中 */
    RADAR_STATE_STATIONARY     /**< 目标静止 */
} radar_state_t;

/**
 * @brief 雷达数据结构体（应用级）
 *
 * 注意：这是"应用级"视图，与底层驱动的 radar_driver_result_t 是两层
 * 不同的数据模型，命名刻意区分以避免符号冲突。
 */
typedef struct {
    radar_state_t state;      /**< 雷达状态 */
    radar_zone_t  zone;       /**< 区域（室内/室外） */
    uint16_t      distance;   /**< 距离（厘米） */
    uint16_t      energy;     /**< 能量值 */
    uint32_t      timestamp;  /**< 时间戳 */
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
 * @brief 获取雷达状态（应用级）
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
