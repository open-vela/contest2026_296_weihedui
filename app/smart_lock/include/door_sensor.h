/****************************************************************************
 * app/smart_lock/include/door_sensor.h
 *
 * 门磁传感器驱动头文件
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change History:
 *   Date        Author   Description
 *   2026-09-12  MemberB  Initial version
 ****************************************************************************/

#ifndef __APP_SMART_LOCK_INCLUDE_DOOR_SENSOR_H
#define __APP_SMART_LOCK_INCLUDE_DOOR_SENSOR_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* GPIO 配置 */

#define DOOR_SENSOR_GPIO_PIN    0  /* GPIO 引脚号 (需要根据实际硬件修改) */
#define DEBOUNCE_TIME_MS        50 /* 消抖时间 (毫秒) */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 门状态枚举 */

typedef enum
{
  DOOR_STATE_UNKNOWN = 0,  /* 未知状态 */
  DOOR_STATE_OPEN,         /* 门打开 */
  DOOR_STATE_CLOSED        /* 门关闭 */
} door_state_t;

/* 锁状态枚举 */

typedef enum
{
  LOCK_STATE_UNKNOWN = 0,  /* 未知状态 */
  LOCK_STATE_UNLOCKED,     /* 未锁定 */
  LOCK_STATE_LOCKED        /* 已锁定 */
} lock_state_t;

/* 门磁状态结构体 */

typedef struct
{
  door_state_t  door_state;    /* 门状态 */
  lock_state_t  lock_state;    /* 锁状态 */
  time_t        timestamp;     /* 状态变化时间戳 */
} door_status_t;

/* 回调函数类型 */

typedef void (*door_status_callback_t)(const door_status_t *status);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化门磁传感器
 *
 * @return 0 成功, 负值失败
 */

int door_sensor_init(void);

/**
 * @brief 注册状态变化回调函数
 *
 * @param callback 回调函数指针
 * @return 0 成功, 负值失败
 */

int door_sensor_register_callback(door_status_callback_t callback);

/**
 * @brief 获取当前门状态
 *
 * @param status 输出参数，存储门状态
 * @return 0 成功, 负值失败
 */

int door_sensor_get_status(door_status_t *status);

/**
 * @brief 设置锁状态 (由主程序调用)
 *
 * @param locked true 锁定, false 解锁
 * @return 0 成功, 负值失败
 */

int door_sensor_set_lock_state(bool locked);

/**
 * @brief 反初始化门磁传感器
 *
 * @return 0 成功, 负值失败
 */

int door_sensor_deinit(void);

#endif /* __APP_SMART_LOCK_INCLUDE_DOOR_SENSOR_H */
