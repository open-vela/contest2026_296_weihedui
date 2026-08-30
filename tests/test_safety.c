/**
 * @file test_safety.c
 * @brief 安全模块测试用例
 *
 * 本文件包含防夹安全模块的单元测试用例。
 *
 * Copyright (C) 2026 weihedui Team
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "safety.h"
#include "door_control.h"

/****************************************************************************
 * Test Helper Macros
 ****************************************************************************/

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name) printf("[FAIL] %s\n", name); return -1

#define TEST_ASSERT(cond, name) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(name); \
        } \
    } while (0)

/****************************************************************************
 * Test Cases
 ****************************************************************************/

/**
 * @brief 测试初始化（默认配置）
 */
static int test_init_default(void)
{
    int ret = safety_init(NULL);
    TEST_ASSERT(ret == 0, "safety_init with default config");

    safety_deinit();

    TEST_PASS("test_init_default");
    return 0;
}

/**
 * @brief 测试初始化（自定义配置）
 */
static int test_init_custom(void)
{
    safety_config_t config = {
        .anti_pinch_threshold = 5,
        .check_interval_ms = 10,
        .detection_range_cm = 150
    };

    int ret = safety_init(&config);
    TEST_ASSERT(ret == 0, "safety_init with custom config");

    safety_deinit();

    TEST_PASS("test_init_custom");
    return 0;
}

/**
 * @brief 测试启动和停止
 */
static int test_start_stop(void)
{
    safety_init(NULL);

    int ret = safety_start();
    TEST_ASSERT(ret == 0, "safety_start");

    /* 等待一小段时间 */
    usleep(100000); /* 100ms */

    ret = safety_stop();
    TEST_ASSERT(ret == 0, "safety_stop");

    safety_deinit();

    TEST_PASS("test_start_stop");
    return 0;
}

/**
 * @brief 测试重复启动
 */
static int test_double_start(void)
{
    safety_init(NULL);

    int ret = safety_start();
    TEST_ASSERT(ret == 0, "first start");

    ret = safety_start();
    TEST_ASSERT(ret == 0, "second start (should warn)");

    safety_stop();
    safety_deinit();

    TEST_PASS("test_double_start");
    return 0;
}

/**
 * @brief 测试雷达状态获取
 */
static int test_radar_status(void)
{
    safety_init(NULL);

    radar_result_t radar;
    int ret = radar_get_status(&radar);
    TEST_ASSERT(ret == 0, "radar_get_status");

    /* 验证雷达状态在有效范围内 */
    TEST_ASSERT(radar.state >= RADAR_STATE_NO_TARGET &&
                radar.state <= RADAR_STATE_STATIONARY,
                "radar state is valid");

    safety_deinit();

    TEST_PASS("test_radar_status");
    return 0;
}

/**
 * @brief 测试防夹触发状态
 */
static int test_anti_pinch_status(void)
{
    safety_init(NULL);

    /* 初始状态应该是未触发 */
    bool triggered = safety_is_anti_pinch_triggered();
    TEST_ASSERT(!triggered, "initial anti-pinch status is false");

    safety_deinit();

    TEST_PASS("test_anti_pinch_status");
    return 0;
}

/**
 * @brief 测试重置功能
 */
static int test_reset(void)
{
    safety_init(NULL);

    /* 重置 */
    safety_reset();

    /* 验证重置后状态 */
    bool triggered = safety_is_anti_pinch_triggered();
    TEST_ASSERT(!triggered, "anti-pinch status is false after reset");

    safety_deinit();

    TEST_PASS("test_reset");
    return 0;
}

/**
 * @brief 测试未初始化时调用
 */
static int test_not_initialized(void)
{
    /* 不初始化直接调用 */
    int ret = safety_start();
    TEST_ASSERT(ret == -1, "safety_start fails when not initialized");

    TEST_PASS("test_not_initialized");
    return 0;
}

/**
 * @brief 测试重复初始化
 */
static int test_double_init(void)
{
    /* 第一次初始化 */
    int ret = safety_init(NULL);
    TEST_ASSERT(ret == 0, "first init");

    /* 第二次初始化（应该成功但打印警告） */
    ret = safety_init(NULL);
    TEST_ASSERT(ret == 0, "second init");

    safety_deinit();

    TEST_PASS("test_double_init");
    return 0;
}

/**
 * @brief 测试与电机控制的集成
 */
static int test_integration(void)
{
    /* 初始化电机控制 */
    motor_config_t motor_config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };
    door_control_init(&motor_config);

    /* 初始化安全模块 */
    safety_init(NULL);
    safety_start();

    /* 开门 */
    motor_control(MOTOR_CMD_OPEN);
    usleep(500000); /* 500ms */
    motor_control(MOTOR_CMD_STOP);

    /* 关门（防夹检测会自动运行） */
    motor_control(MOTOR_CMD_CLOSE);
    usleep(1000000); /* 1秒 */

    /* 停止 */
    motor_control(MOTOR_CMD_STOP);
    safety_stop();

    /* 反初始化 */
    safety_deinit();
    door_control_deinit();

    TEST_PASS("test_integration");
    return 0;
}

/**
 * @brief 测试快速启停
 */
static int test_rapid_start_stop(void)
{
    safety_init(NULL);

    for (int i = 0; i < 5; i++) {
        safety_start();
        usleep(50000); /* 50ms */
        safety_stop();
    }

    safety_deinit();

    TEST_PASS("test_rapid_start_stop");
    return 0;
}

/****************************************************************************
 * Main Test Runner
 ****************************************************************************/

int main(void)
{
    int failed = 0;

    printf("========================================\n");
    printf("  Safety Module Unit Tests\n");
    printf("========================================\n\n");

    failed += test_init_default();
    failed += test_init_custom();
    failed += test_start_stop();
    failed += test_double_start();
    failed += test_radar_status();
    failed += test_anti_pinch_status();
    failed += test_reset();
    failed += test_not_initialized();
    failed += test_double_init();
    failed += test_integration();
    failed += test_rapid_start_stop();

    printf("\n========================================\n");
    if (failed == 0) {
        printf("  ALL TESTS PASSED!\n");
    } else {
        printf("  %d TESTS FAILED!\n", failed);
    }
    printf("========================================\n");

    return failed;
}
