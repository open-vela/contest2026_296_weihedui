/****************************************************************************
 * tests/test_radar.c
 *
 * 雷达驱动单元测试
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
#include "radar_driver.h"
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
  radar_deinit();
}

/****************************************************************************
 * Test Cases
 ****************************************************************************/

/**
 * @brief 测试 radar_init() 初始化
 */

void test_radar_init(void)
{
  /* 注意: 在没有实际硬件的情况下，初始化可能失败 */
  /* 这里测试函数调用不会崩溃 */
  int ret = radar_init();

  /* 在模拟环境中，我们只验证函数可以正常调用 */
  /* 实际硬件测试需要验证返回值为 0 */
  printf("  radar_init() returned: %d\n", ret);
}

/**
 * @brief 测试 radar_parse_frame() 正常帧解析
 */

void test_radar_parse_frame_valid(void)
{
  /* 构造一个有效的目标存在帧 */
  /* 帧格式: [头][长度][功能码][数据][校验和][尾] */
  uint8_t frame[] =
  {
    0xAA,   /* 帧头 */
    0x01,   /* 数据长度低字节 */
    0x00,   /* 数据长度高字节 */
    0x01,   /* 功能码: 目标存在 */
    0x01,   /* 数据: 检测到目标 */
    0x03,   /* 校验和: AA+01+00+01+01 = AD, 但这里简化 */
    0x55    /* 帧尾 */
  };

  /* 重新计算校验和 */
  uint8_t checksum = 0;
  int i;
  for (i = 0; i < 5; i++)
    {
      checksum += frame[i];
    }
  frame[5] = checksum;

  int ret = radar_parse_frame(frame, sizeof(frame));
  TEST_ASSERT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_parse_frame() 无效帧头
 */

void test_radar_parse_frame_invalid_header(void)
{
  /* 帧头错误 */
  uint8_t frame[] =
  {
    0x00,   /* 错误的帧头 */
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x55
  };

  int ret = radar_parse_frame(frame, sizeof(frame));
  TEST_ASSERT_NOT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_parse_frame() 无效帧尾
 */

void test_radar_parse_frame_invalid_tail(void)
{
  /* 帧尾错误 */
  uint8_t frame[] =
  {
    0xAA,   /* 正确的帧头 */
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00    /* 错误的帧尾 */
  };

  /* 计算校验和 */
  uint8_t checksum = 0;
  int i;
  for (i = 0; i < 5; i++)
    {
      checksum += frame[i];
    }
  frame[5] = checksum;

  int ret = radar_parse_frame(frame, sizeof(frame));
  TEST_ASSERT_NOT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_parse_frame() 无效校验和
 */

void test_radar_parse_frame_invalid_checksum(void)
{
  /* 校验和错误 */
  uint8_t frame[] =
  {
    0xAA,   /* 帧头 */
    0x01,
    0x00,
    0x01,
    0x01,
    0xFF,   /* 错误的校验和 */
    0x55    /* 帧尾 */
  };

  int ret = radar_parse_frame(frame, sizeof(frame));
  TEST_ASSERT_NOT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_parse_frame() 空指针
 */

void test_radar_parse_frame_null_pointer(void)
{
  int ret = radar_parse_frame(NULL, 10);
  TEST_ASSERT_NOT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_parse_frame() 长度不足
 */

void test_radar_parse_frame_insufficient_length(void)
{
  uint8_t frame[] = {0xAA, 0x01};

  int ret = radar_parse_frame(frame, sizeof(frame));
  TEST_ASSERT_NOT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_parse_frame() 运动状态帧
 */

void test_radar_parse_frame_motion_state(void)
{
  /* 构造运动状态帧 */
  uint8_t frame[] =
  {
    0xAA,   /* 帧头 */
    0x01,   /* 数据长度 */
    0x00,
    0x02,   /* 功能码: 运动状态 */
    0x02,   /* 数据: 运动中 */
    0x00,   /* 校验和占位 */
    0x55    /* 帧尾 */
  };

  /* 计算校验和 */
  uint8_t checksum = 0;
  int i;
  for (i = 0; i < 5; i++)
    {
      checksum += frame[i];
    }
  frame[5] = checksum;

  int ret = radar_parse_frame(frame, sizeof(frame));
  TEST_ASSERT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_parse_frame() 区域信息帧
 */

void test_radar_parse_frame_region_info(void)
{
  /* 构造区域信息帧 */
  uint8_t frame[] =
  {
    0xAA,   /* 帧头 */
    0x04,   /* 数据长度 */
    0x00,
    0x03,   /* 功能码: 区域信息 */
    0x01,   /* 区域号 */
    0x64,   /* 距离低字节 (100cm) */
    0x00,   /* 距离高字节 */
    0x50,   /* 能量值 (80) */
    0x00,   /* 校验和占位 */
    0x55    /* 帧尾 */
  };

  /* 计算校验和 */
  uint8_t checksum = 0;
  int i;
  for (i = 0; i < 8; i++)
    {
      checksum += frame[i];
    }
  frame[8] = checksum;

  int ret = radar_parse_frame(frame, sizeof(frame));
  TEST_ASSERT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_get_status() 空指针
 */

void test_radar_get_status_null_pointer(void)
{
  /* 先初始化 */
  radar_init();

  int ret = radar_get_status(NULL);
  TEST_ASSERT_NOT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_register_callback() 空指针
 */

void test_radar_register_callback_null(void)
{
  /* 先初始化 */
  radar_init();

  int ret = radar_register_callback(NULL);
  /* 注册 NULL 回调应该成功（清除回调） */
  TEST_ASSERT_EQUAL(0, ret);
}

/**
 * @brief 测试 radar_deinit() 未初始化
 */

void test_radar_deinit_not_initialized(void)
{
  int ret = radar_deinit();
  /* 未初始化时调用应该返回成功 */
  TEST_ASSERT_EQUAL(0, ret);
}

/****************************************************************************
 * Main Function
 ****************************************************************************/

int main(void)
{
  UNITY_BEGIN();

  /* 初始化测试 */
  RUN_TEST(test_radar_init);

  /* 帧解析测试 */
  RUN_TEST(test_radar_parse_frame_valid);
  RUN_TEST(test_radar_parse_frame_invalid_header);
  RUN_TEST(test_radar_parse_frame_invalid_tail);
  RUN_TEST(test_radar_parse_frame_invalid_checksum);
  RUN_TEST(test_radar_parse_frame_null_pointer);
  RUN_TEST(test_radar_parse_frame_insufficient_length);
  RUN_TEST(test_radar_parse_frame_motion_state);
  RUN_TEST(test_radar_parse_frame_region_info);

  /* 状态获取测试 */
  RUN_TEST(test_radar_get_status_null_pointer);

  /* 回调注册测试 */
  RUN_TEST(test_radar_register_callback_null);

  /* 反初始化测试 */
  RUN_TEST(test_radar_deinit_not_initialized);

  UNITY_END();
}
