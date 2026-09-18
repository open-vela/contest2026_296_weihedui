/**
 * @file door_control.c
 * @brief 电机控制模块实现
 *
 * 本文件实现了门锁电机的控制逻辑，包括：
 * - GPIO输出控制（开门/关门/锁门电机、蜂鸣器）
 * - 状态机管理（空闲、开门中、关门中、锁门中）
 * - 超时保护机制
 * - 紧急停止功能
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

#include "door_control.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MOTOR_TASK_STACK_SIZE    4096
#define MOTOR_CHECK_INTERVAL_MS 50

/* GPIO模拟定义（实际硬件需要替换为真实GPIO操作） */
#ifndef CONFIG_USE_REAL_GPIO
#define GPIO_HIGH 1
#define GPIO_LOW  0
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/**
 * @brief 电机控制上下文结构体
 */
typedef struct {
    motor_state_t state;           /**< 当前电机状态 */
    motor_config_t config;         /**< 电机配置 */
    bool initialized;              /**< 是否已初始化 */
    bool timeout_enabled;          /**< 超时保护是否启用 */
    struct timespec start_time;    /**< 操作开始时间 */
    pthread_mutex_t mutex;         /**< 互斥锁 */
    pthread_t timeout_thread;      /**< 超时检测线程 */
    bool timeout_running;          /**< 超时检测线程运行标志 */
} motor_context_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int gpio_write(uint8_t pin, uint8_t value);
static void *timeout_task(void *arg);
static uint32_t get_elapsed_ms(const struct timespec *start);
static int start_timeout_check(void);
static int stop_timeout_check(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static motor_context_t g_motor_ctx = {
    .state = MOTOR_STATE_IDLE,
    .initialized = false,
    .timeout_enabled = true,
    .timeout_running = false,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief GPIO写操作（模拟实现）
 *
 * @param pin GPIO引脚号
 * @param value GPIO电平值
 * @return int 0: 成功, -1: 失败
 */
static int gpio_write(uint8_t pin, uint8_t value)
{
    /* TODO: 替换为实际的GPIO驱动调用 */
    printf("[MOTOR] GPIO %d -> %s\n", pin, value ? "HIGH" : "LOW");
    return 0;
}

/**
 * @brief 获取已经过的时间（毫秒）
 *
 * @param start 起始时间
 * @return uint32_t 经过的毫秒数
 */
static uint32_t get_elapsed_ms(const struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32_t elapsed_ms = (now.tv_sec - start->tv_sec) * 1000 +
                          (now.tv_nsec - start->tv_nsec) / 1000000;
    return elapsed_ms;
}

/**
 * @brief 超时检测任务
 *
 * @param arg 任务参数（未使用）
 * @return void* 返回值（未使用）
 */
static void *timeout_task(void *arg)
{
    (void)arg;

    while (g_motor_ctx.timeout_running) {
        usleep(MOTOR_CHECK_INTERVAL_MS * 1000);

        pthread_mutex_lock(&g_motor_ctx.mutex);

        if (g_motor_ctx.state == MOTOR_STATE_IDLE ||
            g_motor_ctx.state == MOTOR_STATE_ERROR) {
            pthread_mutex_unlock(&g_motor_ctx.mutex);
            continue;
        }

        uint32_t elapsed = get_elapsed_ms(&g_motor_ctx.start_time);
        uint32_t timeout = 0;

        switch (g_motor_ctx.state) {
        case MOTOR_STATE_OPENING:
            timeout = g_motor_ctx.config.open_timeout_ms;
            break;
        case MOTOR_STATE_CLOSING:
            timeout = g_motor_ctx.config.close_timeout_ms;
            break;
        case MOTOR_STATE_LOCKING:
            timeout = g_motor_ctx.config.lock_timeout_ms;
            break;
        default:
            break;
        }

        if (timeout > 0 && elapsed >= timeout) {
            printf("[MOTOR] Timeout! State: %s, Elapsed: %u ms, Timeout: %u ms\n",
                   motor_state_to_string(g_motor_ctx.state), elapsed, timeout);

            /* 停止所有电机 */
            gpio_write(g_motor_ctx.config.open_gpio_pin, GPIO_LOW);
            gpio_write(g_motor_ctx.config.close_gpio_pin, GPIO_LOW);
            gpio_write(g_motor_ctx.config.lock_gpio_pin, GPIO_LOW);

            /* 设置错误状态 */
            g_motor_ctx.state = MOTOR_STATE_ERROR;

            /* 触发蜂鸣器报警 */
            buzzer_alarm(1000);
        }

        pthread_mutex_unlock(&g_motor_ctx.mutex);
    }

    return NULL;
}

/**
 * @brief 启动超时检测
 *
 * @return int 0: 成功, -1: 失败
 */
static int start_timeout_check(void)
{
    if (g_motor_ctx.timeout_running) {
        return 0;
    }

    g_motor_ctx.timeout_running = true;

    int ret = pthread_create(&g_motor_ctx.timeout_thread, NULL,
                             timeout_task, NULL);
    if (ret != 0) {
        printf("[MOTOR] Failed to create timeout thread: %d\n", ret);
        g_motor_ctx.timeout_running = false;
        return -1;
    }

    return 0;
}

/**
 * @brief 停止超时检测
 *
 * @return int 0: 成功, -1: 失败
 */
static int stop_timeout_check(void)
{
    if (!g_motor_ctx.timeout_running) {
        return 0;
    }

    g_motor_ctx.timeout_running = false;
    pthread_join(g_motor_ctx.timeout_thread, NULL);

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int door_control_init(const motor_config_t *config)
{
    if (config == NULL) {
        printf("[MOTOR] Error: config is NULL\n");
        return -1;
    }

    if (g_motor_ctx.initialized) {
        printf("[MOTOR] Warning: already initialized\n");
        return 0;
    }

    /* 保存配置 */
    memcpy(&g_motor_ctx.config, config, sizeof(motor_config_t));

    /* 初始化互斥锁 */
    pthread_mutex_init(&g_motor_ctx.mutex, NULL);

    /* 初始化GPIO */
    gpio_write(config->open_gpio_pin, GPIO_LOW);
    gpio_write(config->close_gpio_pin, GPIO_LOW);
    gpio_write(config->lock_gpio_pin, GPIO_LOW);
    gpio_write(config->buzzer_gpio_pin, GPIO_LOW);

    /* 设置初始状态 */
    g_motor_ctx.state = MOTOR_STATE_IDLE;
    g_motor_ctx.initialized = true;

    /* 启动超时检测 */
    if (g_motor_ctx.timeout_enabled) {
        start_timeout_check();
    }

    printf("[MOTOR] Initialized successfully\n");
    printf("[MOTOR]   Open GPIO: %d\n", config->open_gpio_pin);
    printf("[MOTOR]   Close GPIO: %d\n", config->close_gpio_pin);
    printf("[MOTOR]   Lock GPIO: %d\n", config->lock_gpio_pin);
    printf("[MOTOR]   Buzzer GPIO: %d\n", config->buzzer_gpio_pin);

    return 0;
}

int door_control_deinit(void)
{
    if (!g_motor_ctx.initialized) {
        return 0;
    }

    /* 停止超时检测 */
    stop_timeout_check();

    /* 停止所有电机 */
    pthread_mutex_lock(&g_motor_ctx.mutex);

    gpio_write(g_motor_ctx.config.open_gpio_pin, GPIO_LOW);
    gpio_write(g_motor_ctx.config.close_gpio_pin, GPIO_LOW);
    gpio_write(g_motor_ctx.config.lock_gpio_pin, GPIO_LOW);
    gpio_write(g_motor_ctx.config.buzzer_gpio_pin, GPIO_LOW);

    g_motor_ctx.state = MOTOR_STATE_IDLE;
    g_motor_ctx.initialized = false;

    pthread_mutex_unlock(&g_motor_ctx.mutex);

    /* 销毁互斥锁 */
    pthread_mutex_destroy(&g_motor_ctx.mutex);

    printf("[MOTOR] Deinitialized\n");

    return 0;
}

int motor_control(motor_cmd_t cmd)
{
    if (!g_motor_ctx.initialized) {
        printf("[MOTOR] Error: not initialized\n");
        return -1;
    }

    pthread_mutex_lock(&g_motor_ctx.mutex);

    printf("[MOTOR] Command: %d, Current state: %s\n",
           cmd, motor_state_to_string(g_motor_ctx.state));

    switch (cmd) {
    case MOTOR_CMD_OPEN:
    case MOTOR_CMD_UNLOCK:  /* 集成补充：解锁复用开门电机 */
        /* 停止其他电机 */
        gpio_write(g_motor_ctx.config.close_gpio_pin, GPIO_LOW);
        gpio_write(g_motor_ctx.config.lock_gpio_pin, GPIO_LOW);
        /* 启动开门电机 */
        gpio_write(g_motor_ctx.config.open_gpio_pin, GPIO_HIGH);
        g_motor_ctx.state = MOTOR_STATE_OPENING;
        clock_gettime(CLOCK_MONOTONIC, &g_motor_ctx.start_time);
        break;

    case MOTOR_CMD_CLOSE:
        /* 停止其他电机 */
        gpio_write(g_motor_ctx.config.open_gpio_pin, GPIO_LOW);
        gpio_write(g_motor_ctx.config.lock_gpio_pin, GPIO_LOW);
        /* 启动关门电机 */
        gpio_write(g_motor_ctx.config.close_gpio_pin, GPIO_HIGH);
        g_motor_ctx.state = MOTOR_STATE_CLOSING;
        clock_gettime(CLOCK_MONOTONIC, &g_motor_ctx.start_time);
        break;

    case MOTOR_CMD_LOCK:
        /* 停止其他电机 */
        gpio_write(g_motor_ctx.config.open_gpio_pin, GPIO_LOW);
        gpio_write(g_motor_ctx.config.close_gpio_pin, GPIO_LOW);
        /* 启动锁门电机 */
        gpio_write(g_motor_ctx.config.lock_gpio_pin, GPIO_HIGH);
        g_motor_ctx.state = MOTOR_STATE_LOCKING;
        clock_gettime(CLOCK_MONOTONIC, &g_motor_ctx.start_time);
        break;

    case MOTOR_CMD_STOP:
    case MOTOR_CMD_EMERGENCY_STOP:
        /* 停止所有电机 */
        gpio_write(g_motor_ctx.config.open_gpio_pin, GPIO_LOW);
        gpio_write(g_motor_ctx.config.close_gpio_pin, GPIO_LOW);
        gpio_write(g_motor_ctx.config.lock_gpio_pin, GPIO_LOW);
        g_motor_ctx.state = MOTOR_STATE_IDLE;
        break;

    default:
        printf("[MOTOR] Error: unknown command %d\n", cmd);
        pthread_mutex_unlock(&g_motor_ctx.mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_motor_ctx.mutex);

    printf("[MOTOR] New state: %s\n", motor_state_to_string(g_motor_ctx.state));

    return 0;
}

motor_state_t motor_get_state(void)
{
    return g_motor_ctx.state;
}

const char *motor_state_to_string(motor_state_t state)
{
    switch (state) {
    case MOTOR_STATE_IDLE:    return "IDLE";
    case MOTOR_STATE_OPENING: return "OPENING";
    case MOTOR_STATE_CLOSING: return "CLOSING";
    case MOTOR_STATE_LOCKING: return "LOCKING";
    case MOTOR_STATE_ERROR:   return "ERROR";
    default:                  return "UNKNOWN";
    }
}

int buzzer_alarm(uint32_t duration_ms)
{
    if (!g_motor_ctx.initialized) {
        return -1;
    }

    printf("[BUZZER] Alarm for %u ms\n", duration_ms);

    gpio_write(g_motor_ctx.config.buzzer_gpio_pin, GPIO_HIGH);
    usleep(duration_ms * 1000);
    gpio_write(g_motor_ctx.config.buzzer_gpio_pin, GPIO_LOW);

    return 0;
}

int buzzer_stop(void)
{
    if (!g_motor_ctx.initialized) {
        return -1;
    }

    printf("[BUZZER] Stop\n");

    gpio_write(g_motor_ctx.config.buzzer_gpio_pin, GPIO_LOW);

    return 0;
}
