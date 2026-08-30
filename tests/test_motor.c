/**
 * @file test_motor.c
 * @brief 电机控制模块测试用例
 *
 * 本文件包含电机控制模块的单元测试用例。
 *
 * Copyright (C) 2026 weihedui Team
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

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
 * @brief 测试初始化
 */
static int test_init(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    int ret = door_control_init(&config);
    TEST_ASSERT(ret == 0, "door_control_init");

    motor_state_t state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_IDLE, "initial state is IDLE");

    door_control_deinit();

    TEST_PASS("test_init");
    return 0;
}

/**
 * @brief 测试开门命令
 */
static int test_motor_open(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    door_control_init(&config);

    int ret = motor_control(MOTOR_CMD_OPEN);
    TEST_ASSERT(ret == 0, "motor_control OPEN");

    motor_state_t state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_OPENING, "state is OPENING");

    motor_control(MOTOR_CMD_STOP);
    door_control_deinit();

    TEST_PASS("test_motor_open");
    return 0;
}

/**
 * @brief 测试关门命令
 */
static int test_motor_close(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    door_control_init(&config);

    int ret = motor_control(MOTOR_CMD_CLOSE);
    TEST_ASSERT(ret == 0, "motor_control CLOSE");

    motor_state_t state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_CLOSING, "state is CLOSING");

    motor_control(MOTOR_CMD_STOP);
    door_control_deinit();

    TEST_PASS("test_motor_close");
    return 0;
}

/**
 * @brief 测试锁门命令
 */
static int test_motor_lock(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    door_control_init(&config);

    int ret = motor_control(MOTOR_CMD_LOCK);
    TEST_ASSERT(ret == 0, "motor_control LOCK");

    motor_state_t state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_LOCKING, "state is LOCKING");

    motor_control(MOTOR_CMD_STOP);
    door_control_deinit();

    TEST_PASS("test_motor_lock");
    return 0;
}

/**
 * @brief 测试停止命令
 */
static int test_motor_stop(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    door_control_init(&config);

    /* 先开门 */
    motor_control(MOTOR_CMD_OPEN);
    motor_state_t state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_OPENING, "state is OPENING after OPEN");

    /* 停止 */
    int ret = motor_control(MOTOR_CMD_STOP);
    TEST_ASSERT(ret == 0, "motor_control STOP");

    state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_IDLE, "state is IDLE after STOP");

    door_control_deinit();

    TEST_PASS("test_motor_stop");
    return 0;
}

/**
 * @brief 测试紧急停止命令
 */
static int test_motor_emergency_stop(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    door_control_init(&config);

    /* 先关门 */
    motor_control(MOTOR_CMD_CLOSE);
    motor_state_t state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_CLOSING, "state is CLOSING after CLOSE");

    /* 紧急停止 */
    int ret = motor_control(MOTOR_CMD_EMERGENCY_STOP);
    TEST_ASSERT(ret == 0, "motor_control EMERGENCY_STOP");

    state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_IDLE, "state is IDLE after EMERGENCY_STOP");

    door_control_deinit();

    TEST_PASS("test_motor_emergency_stop");
    return 0;
}

/**
 * @brief 测试状态字符串转换
 */
static int test_state_to_string(void)
{
    const char *str;

    str = motor_state_to_string(MOTOR_STATE_IDLE);
    TEST_ASSERT(strcmp(str, "IDLE") == 0, "IDLE string");

    str = motor_state_to_string(MOTOR_STATE_OPENING);
    TEST_ASSERT(strcmp(str, "OPENING") == 0, "OPENING string");

    str = motor_state_to_string(MOTOR_STATE_CLOSING);
    TEST_ASSERT(strcmp(str, "CLOSING") == 0, "CLOSING string");

    str = motor_state_to_string(MOTOR_STATE_LOCKING);
    TEST_ASSERT(strcmp(str, "LOCKING") == 0, "LOCKING string");

    str = motor_state_to_string(MOTOR_STATE_ERROR);
    TEST_ASSERT(strcmp(str, "ERROR") == 0, "ERROR string");

    TEST_PASS("test_state_to_string");
    return 0;
}

/**
 * @brief 测试蜂鸣器
 */
static int test_buzzer(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    door_control_init(&config);

    /* 测试蜂鸣器报警 */
    int ret = buzzer_alarm(100);
    TEST_ASSERT(ret == 0, "buzzer_alarm");

    /* 测试停止蜂鸣器 */
    ret = buzzer_stop();
    TEST_ASSERT(ret == 0, "buzzer_stop");

    door_control_deinit();

    TEST_PASS("test_buzzer");
    return 0;
}

/**
 * @brief 测试未初始化时调用
 */
static int test_not_initialized(void)
{
    /* 不初始化直接调用 */
    int ret = motor_control(MOTOR_CMD_OPEN);
    TEST_ASSERT(ret == -1, "motor_control fails when not initialized");

    motor_state_t state = motor_get_state();
    TEST_ASSERT(state == MOTOR_STATE_IDLE, "state is IDLE when not initialized");

    TEST_PASS("test_not_initialized");
    return 0;
}

/**
 * @brief 测试重复初始化
 */
static int test_double_init(void)
{
    motor_config_t config = {
        .open_gpio_pin = 26,
        .close_gpio_pin = 27,
        .lock_gpio_pin = 28,
        .buzzer_gpio_pin = 29,
        .open_timeout_ms = 10000,
        .close_timeout_ms = 15000,
        .lock_timeout_ms = 5000
    };

    /* 第一次初始化 */
    int ret = door_control_init(&config);
    TEST_ASSERT(ret == 0, "first init");

    /* 第二次初始化（应该成功但打印警告） */
    ret = door_control_init(&config);
    TEST_ASSERT(ret == 0, "second init");

    door_control_deinit();

    TEST_PASS("test_double_init");
    return 0;
}

/****************************************************************************
 * Main Test Runner
 ****************************************************************************/

int main(void)
{
    int failed = 0;

    printf("========================================\n");
    printf("  Motor Control Unit Tests\n");
    printf("========================================\n\n");

    failed += test_init();
    failed += test_motor_open();
    failed += test_motor_close();
    failed += test_motor_lock();
    failed += test_motor_stop();
    failed += test_motor_emergency_stop();
    failed += test_state_to_string();
    failed += test_buzzer();
    failed += test_not_initialized();
    failed += test_double_init();

    printf("\n========================================\n");
    if (failed == 0) {
        printf("  ALL TESTS PASSED!\n");
    } else {
        printf("  %d TESTS FAILED!\n", failed);
    }
    printf("========================================\n");

    return failed;
}
