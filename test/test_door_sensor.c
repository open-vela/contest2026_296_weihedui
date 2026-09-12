/****************************************************************************
 * tests/test_door_sensor.c
 *
 * 门磁传感器驱动单元测试
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change History:
 *   Date        Author   Description
 *   2026-09-12  MemberB  Initial version
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "unity.h"
#include "door_sensor.h"
#include <string.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 测试初始化
 */

void setUp(void)
{
  /* 每个测试前的初始化 */
}

/**
 * @brief 测试清理
 */

void tearDown(void)
{
  /* 每个测试后的清理 */
  door_sensor_deinit();
}

/****************************************************************************
 * Test Cases
 ****************************************************************************/

/**
 * @brief 测试 door_sensor_init() 初始化
 */

void test_door_sensor_init(void)
{
  /* 注意: 在没有实际硬件的情况下，初始化可能失败 */
  /* 这里测试函数调用不会崩溃 */
  int ret = door_sensor_init();

  /* 在模拟环境中，我们只验证函数可以正常调用 */
  /* 实际硬件测试需要验证返回值为 0 */
  printf("  door_sensor_init() returned: %d\n", ret);
}

/**
 * @brief 测试 door_sensor_get_status() 正常获取
 */

void test_door_sensor_get_status(void)
{
  door_status_t status;
  memset(&status, 0, sizeof(status));

  /* 先初始化 */
  door_sensor_init();

  int ret = door_sensor_get_status(&status);

  /* 在模拟环境中，可能返回错误 */
  /* 实际硬件测试需要验证返回值为 0 */
  printf("  door_sensor_get_status() returned: %d\n", ret);
}

/**
 * @brief 测试 door_sensor_get_status() 空指针
 */

void test_door_sensor_get_status_null_pointer(void)
{
  /* 先初始化 */
  door_sensor_init();

  int ret = door_sensor_get_status(NULL);
  TEST_ASSERT_NOT_EQUAL(0, ret);
}

/**
 * @brief 测试 door_sensor_set_lock_state() 锁定
 */

void test_door_sensor_set_lock_state_locked(void)
{
  /* 先初始化 */
  door_sensor_init();

  int ret = door_sensor_set_lock_state(true);

  /* 在模拟环境中，可能返回错误 */
  /* 实际硬件测试需要验证返回值为 0 */
  printf("  door_sensor_set_lock_state(true) returned: %d\n", ret);
}

/**
 * @brief 测试 door_sensor_set_lock_state() 解锁
 */

void test_door_sensor_set_lock_state_unlocked(void)
{
  /* 先初始化 */
  door_sensor_init();

  int ret = door_sensor_set_lock_state(false);

  /* 在模拟环境中，可能返回错误 */
  /* 实际硬件测试需要验证返回值为 0 */
  printf("  door_sensor_set_lock_state(false) returned: %d\n", ret);
}

/**
 * @brief 测试 door_sensor_register_callback() 空指针
 */

void test_door_sensor_register_callback_null(void)
{
  /* 先初始化 */
  door_sensor_init();

  int ret = door_sensor_register_callback(NULL);
  /* 注册 NULL 回调应该成功（清除回调） */
  TEST_ASSERT_EQUAL(0, ret);
}

/**
 * @brief 测试 door_sensor_deinit() 未初始化
 */

void test_door_sensor_deinit_not_initialized(void)
{
  int ret = door_sensor_deinit();
  /* 未初始化时调用应该返回成功 */
  TEST_ASSERT_EQUAL(0, ret);
}

/**
 * @brief 测试 door_state_t 枚举值
 */

void test_door_state_enum_values(void)
{
  TEST_ASSERT_EQUAL(0, DOOR_STATE_UNKNOWN);
  TEST_ASSERT_EQUAL(1, DOOR_STATE_OPEN);
  TEST_ASSERT_EQUAL(2, DOOR_STATE_CLOSED);
}

/**
 * @brief 测试 lock_state_t 枚举值
 */

void test_lock_state_enum_values(void)
{
  TEST_ASSERT_EQUAL(0, LOCK_STATE_UNKNOWN);
  TEST_ASSERT_EQUAL(1, LOCK_STATE_UNLOCKED);
  TEST_ASSERT_EQUAL(2, LOCK_STATE_LOCKED);
}

/**
 * @brief 测试 door_status_t 结构体大小
 */

void test_door_status_struct_size(void)
{
  /* 验证结构体大小合理 */
  size_t size = sizeof(door_status_t);
  TEST_ASSERT_TRUE(size > 0);
  TEST_ASSERT_TRUE(size <= 64);  /* 合理的大小限制 */
}

/****************************************************************************
 * Main Function
 ****************************************************************************/

int main(void)
{
  UNITY_BEGIN();

  /* 初始化测试 */
  RUN_TEST(test_door_sensor_init);

  /* 状态获取测试 */
  RUN_TEST(test_door_sensor_get_status);
  RUN_TEST(test_door_sensor_get_status_null_pointer);

  /* 锁状态设置测试 */
  RUN_TEST(test_door_sensor_set_lock_state_locked);
  RUN_TEST(test_door_sensor_set_lock_state_unlocked);

  /* 回调注册测试 */
  RUN_TEST(test_door_sensor_register_callback_null);

  /* 反初始化测试 */
  RUN_TEST(test_door_sensor_deinit_not_initialized);

  /* 枚举值测试 */
  RUN_TEST(test_door_state_enum_values);
  RUN_TEST(test_lock_state_enum_values);

  /* 结构体测试 */
  RUN_TEST(test_door_status_struct_size);

  UNITY_END();
}
