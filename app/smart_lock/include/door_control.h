/**
 * @file door_control.h
 * @brief 电机控制模块头文件
 *
 * 本文件定义了门锁电机控制的接口和数据结构。
 *
 * Copyright (C) 2026 weihedui Team
 * Licensed under the Apache License, Version 2.0
 */

#ifndef __DOOR_CONTROL_H__
#define __DOOR_CONTROL_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 电机命令定义
 */
typedef enum {
    MOTOR_CMD_OPEN,           /**< 开门命令 */
    MOTOR_CMD_CLOSE,          /**< 关门命令 */
    MOTOR_CMD_LOCK,           /**< 锁门命令 */
    MOTOR_CMD_STOP,           /**< 停止命令 */
    MOTOR_CMD_EMERGENCY_STOP, /**< 紧急停止命令 */
    MOTOR_CMD_UNLOCK          /**< 解锁命令（集成补充：复用开门电机）*/
} motor_cmd_t;

/**
 * @brief 电机状态定义
 */
typedef enum {
    MOTOR_STATE_IDLE,         /**< 空闲状态 */
    MOTOR_STATE_OPENING,      /**< 开门中 */
    MOTOR_STATE_CLOSING,      /**< 关门中 */
    MOTOR_STATE_LOCKING,      /**< 锁门中 */
    MOTOR_STATE_ERROR         /**< 错误状态 */
} motor_state_t;

/**
 * @brief 电机配置结构体
 */
typedef struct {
    uint8_t open_gpio_pin;    /**< 开门电机GPIO引脚 */
    uint8_t close_gpio_pin;   /**< 关门电机GPIO引脚 */
    uint8_t lock_gpio_pin;    /**< 锁门电机GPIO引脚 */
    uint8_t buzzer_gpio_pin;  /**< 蜂鸣器GPIO引脚 */
    uint32_t open_timeout_ms; /**< 开门超时时间(ms) */
    uint32_t close_timeout_ms;/**< 关门超时时间(ms) */
    uint32_t lock_timeout_ms; /**< 锁门超时时间(ms) */
} motor_config_t;

/**
 * @brief 初始化电机控制模块
 *
 * @param config 电机配置参数
 * @return int 0: 成功, -1: 失败
 */
int door_control_init(const motor_config_t *config);

/**
 * @brief 反初始化电机控制模块
 *
 * @return int 0: 成功, -1: 失败
 */
int door_control_deinit(void);

/**
 * @brief 控制电机
 *
 * @param cmd 电机命令
 * @return int 0: 成功, -1: 失败
 */
int motor_control(motor_cmd_t cmd);

/**
 * @brief 获取电机状态
 *
 * @return motor_state_t 当前电机状态
 */
motor_state_t motor_get_state(void);

/**
 * @brief 获取电机状态字符串
 *
 * @param state 电机状态
 * @return const char* 状态字符串
 */
const char *motor_state_to_string(motor_state_t state);

/**
 * @brief 启动蜂鸣器报警
 *
 * @param duration_ms 报警持续时间(ms)
 * @return int 0: 成功, -1: 失败
 */
int buzzer_alarm(uint32_t duration_ms);

/**
 * @brief 停止蜂鸣器报警
 *
 * @return int 0: 成功, -1: 失败
 */
int buzzer_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __DOOR_CONTROL_H__ */
