/****************************************************************************
 * tests/unity.h
 *
 * Unity 测试框架简化版本
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __TESTS_UNITY_H
#define __TESTS_UNITY_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 测试宏定义 */

#define UNITY_BEGIN()  unity_begin()
#define UNITY_END()    return unity_end()

#define RUN_TEST(func) unity_run_test(func, #func)

#define TEST_ASSERT_EQUAL(expected, actual) \
  unity_assert_equal_int(expected, actual, __FILE__, __LINE__)

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
  unity_assert_not_equal_int(expected, actual, __FILE__, __LINE__)

#define TEST_ASSERT_TRUE(condition) \
  unity_assert_true(condition, __FILE__, __LINE__)

#define TEST_ASSERT_FALSE(condition) \
  unity_assert_false(condition, __FILE__, __LINE__)

#define TEST_ASSERT_NULL(pointer) \
  unity_assert_null(pointer, __FILE__, __LINE__)

#define TEST_ASSERT_NOT_NULL(pointer) \
  unity_assert_not_null(pointer, __FILE__, __LINE__)

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

static int unity_tests_run = 0;
static int unity_tests_passed = 0;
static int unity_tests_failed = 0;

static inline int unity_begin(void)
{
  unity_tests_run = 0;
  unity_tests_passed = 0;
  unity_tests_failed = 0;
  printf("\n=== Unity Test Framework ===\n\n");
  return 0;
}

static inline int unity_end(void)
{
  printf("\n=== Test Results ===\n");
  printf("Total:  %d\n", unity_tests_run);
  printf("Passed: %d\n", unity_tests_passed);
  printf("Failed: %d\n", unity_tests_failed);
  printf("====================\n\n");
  return unity_tests_failed;
}

static inline void unity_run_test(void (*func)(void), const char *name)
{
  printf("Running: %s... ", name);
  unity_tests_run++;

  func();

  if (unity_tests_failed == unity_tests_run - 1)
    {
      printf("FAILED\n");
    }
  else
    {
      printf("PASSED\n");
      unity_tests_passed++;
    }
}

static inline void unity_assert_equal_int(int expected, int actual,
                                           const char *file, int line)
{
  if (expected != actual)
    {
      printf("\n  Assert failed at %s:%d\n", file, line);
      printf("  Expected: %d\n", expected);
      printf("  Actual:   %d\n", actual);
      unity_tests_failed++;
    }
}

static inline void unity_assert_not_equal_int(int expected, int actual,
                                               const char *file, int line)
{
  if (expected == actual)
    {
      printf("\n  Assert failed at %s:%d\n", file, line);
      printf("  Expected not: %d\n", expected);
      printf("  Actual:       %d\n", actual);
      unity_tests_failed++;
    }
}

static inline void unity_assert_true(int condition,
                                      const char *file, int line)
{
  if (!condition)
    {
      printf("\n  Assert failed at %s:%d\n", file, line);
      printf("  Expected: TRUE\n");
      printf("  Actual:   FALSE\n");
      unity_tests_failed++;
    }
}

static inline void unity_assert_false(int condition,
                                       const char *file, int line)
{
  if (condition)
    {
      printf("\n  Assert failed at %s:%d\n", file, line);
      printf("  Expected: FALSE\n");
      printf("  Actual:   TRUE\n");
      unity_tests_failed++;
    }
}

static inline void unity_assert_null(const void *pointer,
                                      const char *file, int line)
{
  if (pointer != NULL)
    {
      printf("\n  Assert failed at %s:%d\n", file, line);
      printf("  Expected: NULL\n");
      printf("  Actual:   %p\n", pointer);
      unity_tests_failed++;
    }
}

static inline void unity_assert_not_null(const void *pointer,
                                          const char *file, int line)
{
  if (pointer == NULL)
    {
      printf("\n  Assert failed at %s:%d\n", file, line);
      printf("  Expected: NOT NULL\n");
      printf("  Actual:   NULL\n");
      unity_tests_failed++;
    }
}

#endif /* __TESTS_UNITY_H */
