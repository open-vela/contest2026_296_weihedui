/****************************************************************************
 * app/smart_lock/include/radar_driver.h
 *
 * HLK-LD2410 毫米波雷达驱动头文件
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change History:
 *   Date        Author   Description
 *   2026-09-12  MemberB  Initial version
 ****************************************************************************/

#ifndef __APP_SMART_LOCK_INCLUDE_RADAR_DRIVER_H
#define __APP_SMART_LOCK_INCLUDE_RADAR_DRIVER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* HLK-LD2410 帧格式定义 */

#define RADAR_FRAME_HEADER        0xAA  /* 帧头标识 */
#define RADAR_FRAME_TAIL          0x55  /* 帧尾标识 */
#define RADAR_FRAME_MIN_LEN       10    /* 最小帧长度 */
#define RADAR_BUFFER_SIZE         1024  /* 环形缓冲区大小 */

/* 功能码定义 */

#define RADAR_FUNC_TARGET_EXIST   0x01  /* 目标存在状态 */
#define RADAR_FUNC_MOTION_STATE   0x02  /* 运动状态 */
#define RADAR_FUNC_REGION_INFO    0x03  /* 区域信息 */

/* 运动状态定义 */

#define RADAR_MOTION_NONE         0x00  /* 无目标 */
#define RADAR_MOTION_STATIONARY   0x01  /* 静止目标 */
#define RADAR_MOTION_MOVING       0x02  /* 运动目标 */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 帧解析状态机状态 */

typedef enum
{
  PARSE_STATE_IDLE,        /* 空闲状态，等待帧头 */
  PARSE_STATE_HEADER,      /* 已收到帧头 */
  PARSE_STATE_LENGTH,      /* 解析长度 */
  PARSE_STATE_FUNC_CODE,   /* 解析功能码 */
  PARSE_STATE_DATA,        /* 解析数据区 */
  PARSE_STATE_CHECKSUM,    /* 校验和验证 */
  PARSE_STATE_TAIL         /* 帧尾验证 */
} parse_state_t;

/* 雷达数据结果结构体 */

typedef struct
{
  bool     target_detected;    /* 是否检测到目标 */
  uint8_t  motion_state;       /* 运动状态 */
  uint16_t target_distance;    /* 目标距离 (cm) */
  uint8_t  energy_value;       /* 能量值 */
  uint32_t timestamp;          /* 时间戳 */
} radar_result_t;

/* 环形缓冲区结构体 */

typedef struct
{
  uint8_t buffer[RADAR_BUFFER_SIZE];  /* 缓冲区 */
  size_t  head;                        /* 写入位置 */
  size_t  tail;                        /* 读取位置 */
  size_t  count;                       /* 当前数据量 */
} ring_buffer_t;

/* 回调函数类型 */

typedef void (*radar_callback_t)(const radar_result_t *result);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化雷达驱动
 *
 * @return 0 成功, 负值失败
 */

int radar_init(void);

/**
 * @brief 注册数据回调函数
 *
 * @param callback 回调函数指针
 * @return 0 成功, 负值失败
 */

int radar_register_callback(radar_callback_t callback);

/**
 * @brief 获取当前雷达状态
 *
 * @param result 输出参数，存储雷达数据
 * @return 0 成功, 负值失败
 */

int radar_get_status(radar_result_t *result);

/**
 * @brief 解析雷达数据帧
 *
 * @param frame 帧数据指针
 * @param len 帧数据长度
 * @return 0 成功, 负值失败
 */

int radar_parse_frame(const uint8_t *frame, size_t len);

/**
 * @brief 反初始化雷达驱动
 *
 * @return 0 成功, 负值失败
 */

int radar_deinit(void);

#endif /* __APP_SMART_LOCK_INCLUDE_RADAR_DRIVER_H */
