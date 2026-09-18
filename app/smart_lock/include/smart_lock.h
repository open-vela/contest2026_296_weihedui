/****************************************************************************
 * app/smart_lock/include/smart_lock.h
 *
 * 智锁卫士 - 应用统一头文件
 *
 * 汇总各子模块接口，供 ble_service.c / agent_tools.c / smart_lock_main.c 使用。
 * 该文件此前缺失，导致 ble_service.c 与 agent_tools.c 无法编译。
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __APP_SMART_LOCK_INCLUDE_SMART_LOCK_H
#define __APP_SMART_LOCK_INCLUDE_SMART_LOCK_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/* 子模块接口 */

#include "radar_driver.h"   /* 底层雷达驱动：radar_driver_result_t / radar_driver_get_status() */
#include "door_sensor.h"    /* 门磁传感器：door_sensor_init() / door_sensor_get_status()        */
#include "door_control.h"   /* 电机与蜂鸣器：motor_control() / MOTOR_CMD_*                       */
#include "safety.h"         /* 防夹安全：应用级 radar_result_t / radar_get_status()             */

/****************************************************************************
 * BLE 服务接口 (src/ble_service.c)
 ****************************************************************************/

int     ble_service_init(void);
int     ble_start_advertising(void);
int     ble_stop_advertising(void);
int     ble_notify_door_state(uint8_t state);
int     process_ble_command(uint8_t cmd);

/**
 * @brief 查询 BLE 当前是否有活动连接
 *
 * 该函数被 agent_tools.c 调用，但此前从未实现，导致链接失败。
 *
 * @return true 已连接, false 未连接
 */

bool    ble_is_connected(void);

/**
 * @brief 获取当前门状态
 *
 * @return uint8_t 0: 关闭, 1: 开启
 */

uint8_t get_door_state(void);

/****************************************************************************
 * AI Agent 工具 (src/agent_tools.c)
 *
 * docs/communication/agent_guide.md 约定的五个工具。由 src/agent_main.c 里
 * 的 `agent` NSH 命令分发调用；工具各自负责打印 JSON 结果。
 ****************************************************************************/

int     tool_radar_check(int argc, char **argv);
int     tool_door_sensor_read(int argc, char **argv);
int     tool_motor_control(int argc, char **argv);
int     tool_set_timeout(int argc, char **argv);
int     tool_system_status(int argc, char **argv);

/****************************************************************************
 * 应用入口 (src/smart_lock_main.c)
 ****************************************************************************/

int smart_lock_main(int argc, char *argv[]);

/****************************************************************************
 * 自动关门超时 (src/smart_lock_main.c)
 ****************************************************************************/

/**
 * @brief 设置自动关门超时时间（秒）
 *
 * agent_tools.c 的 tool_set_timeout() 依赖本函数；原提交中无任何实现。
 *
 * @param seconds 超时秒数，有效范围 1-300
 * @return int 0: 成功, -EINVAL: 参数非法
 */

int set_auto_close_timeout(int seconds);

/**
 * @brief 读取当前自动关门超时时间
 *
 * @return int 当前超时秒数
 */

int get_auto_close_timeout(void);

#endif /* __APP_SMART_LOCK_INCLUDE_SMART_LOCK_H */
