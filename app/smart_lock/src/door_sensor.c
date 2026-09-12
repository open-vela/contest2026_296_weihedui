/****************************************************************************
 * app/smart_lock/src/door_sensor.c
 *
 * 门磁传感器驱动实现
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change History:
 *   Date        Author   Description
 *   2026-09-12  MemberB  Initial version - GPIO interrupt implementation
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "door_sensor.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GPIO_EXPORT_PATH     "/sys/class/gpio/export"
#define GPIO_DIRECTION_PATH  "/sys/class/gpio/gpio%d/direction"
#define GPIO_VALUE_PATH      "/sys/class/gpio/gpio%d/value"
#define GPIO_EDGE_PATH       "/sys/class/gpio/gpio%d/edge"
#define GPIO_BUFFER_SIZE     64

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* 驱动上下文结构体 */

typedef struct
{
  int                      gpio_fd;           /* GPIO 文件描述符 */
  bool                     initialized;       /* 初始化标志 */
  bool                     running;           /* 监控线程运行标志 */
  pthread_t                monitor_thread;    /* 监控线程 */
  pthread_mutex_t          mutex;             /* 互斥锁 */
  door_status_t            status;            /* 当前状态 */
  door_status_callback_t   callback;          /* 回调函数 */
  uint32_t                 last_irq_time;     /* 上次中断时间 (用于消抖) */
} door_sensor_context_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  gpio_init(int pin);
static int  gpio_set_direction(int pin, const char *direction);
static int  gpio_set_edge(int pin, const char *edge);
static int  gpio_read_value(int pin);
static void *monitor_thread_func(void *arg);
static uint32_t get_tick_ms(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static door_sensor_context_t g_door_ctx;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 获取系统 tick (毫秒)
 *
 * @return 当前时间 (毫秒)
 */

static uint32_t get_tick_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief 初始化 GPIO
 *
 * @param pin GPIO 引脚号
 * @return 0 成功, 负值失败
 */

static int gpio_init(int pin)
{
  int fd;
  char buffer[GPIO_BUFFER_SIZE];
  int ret;

  /* 导出 GPIO */

  fd = open(GPIO_EXPORT_PATH, O_WRONLY);
  if (fd < 0)
    {
      perror("Failed to open GPIO export");
      return -errno;
    }

  ret = snprintf(buffer, sizeof(buffer), "%d", pin);
  if (write(fd, buffer, ret) < 0)
    {
      /* GPIO 可能已经导出，忽略错误 */
    }

  close(fd);

  /* 设置方向为输入 */

  ret = gpio_set_direction(pin, "in");
  if (ret < 0)
    {
      return ret;
    }

  /* 设置中断触发方式为双边沿 */

  ret = gpio_set_edge(pin, "both");
  if (ret < 0)
    {
      return ret;
    }

  return 0;
}

/**
 * @brief 设置 GPIO 方向
 *
 * @param pin GPIO 引脚号
 * @param direction 方向 ("in" 或 "out")
 * @return 0 成功, 负值失败
 */

static int gpio_set_direction(int pin, const char *direction)
{
  int fd;
  char path[GPIO_BUFFER_SIZE];

  snprintf(path, sizeof(path), GPIO_DIRECTION_PATH, pin);

  fd = open(path, O_WRONLY);
  if (fd < 0)
    {
      perror("Failed to open GPIO direction");
      return -errno;
    }

  if (write(fd, direction, strlen(direction)) < 0)
    {
      perror("Failed to set GPIO direction");
      close(fd);
      return -errno;
    }

  close(fd);
  return 0;
}

/**
 * @brief 设置 GPIO 边沿触发方式
 *
 * @param pin GPIO 引脚号
 * @param edge 边沿类型 ("none", "rising", "falling", "both")
 * @return 0 成功, 负值失败
 */

static int gpio_set_edge(int pin, const char *edge)
{
  int fd;
  char path[GPIO_BUFFER_SIZE];

  snprintf(path, sizeof(path), GPIO_EDGE_PATH, pin);

  fd = open(path, O_WRONLY);
  if (fd < 0)
    {
      perror("Failed to open GPIO edge");
      return -errno;
    }

  if (write(fd, edge, strlen(edge)) < 0)
    {
      perror("Failed to set GPIO edge");
      close(fd);
      return -errno;
    }

  close(fd);
  return 0;
}

/**
 * @brief 读取 GPIO 值
 *
 * @param pin GPIO 引脚号
 * @return GPIO 值 (0 或 1), 负值表示失败
 */

static int gpio_read_value(int pin)
{
  int fd;
  char path[GPIO_BUFFER_SIZE];
  char value;

  snprintf(path, sizeof(path), GPIO_VALUE_PATH, pin);

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      perror("Failed to open GPIO value");
      return -errno;
    }

  if (read(fd, &value, 1) < 0)
    {
      perror("Failed to read GPIO value");
      close(fd);
      return -errno;
    }

  close(fd);
  return (value == '1') ? 1 : 0;
}

/**
 * @brief GPIO 中断回调函数
 *
 * @param irq 中断号
 * @param context 上下文
 * @param arg 参数
 * @return IRQ 处理结果
 */

static int door_sensor_isr(int irq, void *context, void *arg)
{
  (void)irq;
  (void)context;
  (void)arg;

  uint32_t now = get_tick_ms();

  /* 消抖处理: 50ms 内忽略重复中断 */
  if (now - g_door_ctx.last_irq_time < DEBOUNCE_TIME_MS)
    {
      return 0;
    }

  g_door_ctx.last_irq_time = now;

  /* 读取 GPIO 状态 */
  int gpio_value = gpio_read_value(DOOR_SENSOR_GPIO_PIN);
  if (gpio_value < 0)
    {
      return -1;
    }

  /* 更新门状态 */
  pthread_mutex_lock(&g_door_ctx.mutex);

  g_door_ctx.status.door_state = gpio_value ?
    DOOR_STATE_CLOSED : DOOR_STATE_OPEN;
  g_door_ctx.status.timestamp = time(NULL);

  /* 通知回调 */
  if (g_door_ctx.callback)
    {
      g_door_ctx.callback(&g_door_ctx.status);
    }

  pthread_mutex_unlock(&g_door_ctx.mutex);

  return 0;
}

/**
 * @brief 门磁监控线程函数
 *
 * @param arg 线程参数
 * @return NULL
 */

static void *monitor_thread_func(void *arg)
{
  (void)arg;
  int ret;
  fd_set fds;
  struct timeval timeout;
  char buffer[GPIO_BUFFER_SIZE];

  while (g_door_ctx.running)
    {
      /* 使用 poll 等待 GPIO 中断 */

      FD_ZERO(&fds);
      FD_SET(g_door_ctx.gpio_fd, &fds);

      timeout.tv_sec = 1;  /* 1 秒超时 */
      timeout.tv_usec = 0;

      ret = select(g_door_ctx.gpio_fd + 1, &fds, NULL, NULL, &timeout);

      if (ret > 0 && FD_ISSET(g_door_ctx.gpio_fd, &fds))
        {
          /* 读取 GPIO 值以清除中断 */
          lseek(g_door_ctx.gpio_fd, 0, SEEK_SET);
          read(g_door_ctx.gpio_fd, buffer, sizeof(buffer));

          /* 处理中断 */
          door_sensor_isr(0, NULL, NULL);
        }
      else if (ret < 0)
        {
          if (errno != EINTR)
            {
              perror("select error");
              break;
            }
        }
    }

  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * @brief 初始化门磁传感器
 *
 * @return 0 成功, 负值失败
 */

int door_sensor_init(void)
{
  int ret;
  char path[GPIO_BUFFER_SIZE];

  if (g_door_ctx.initialized)
    {
      fprintf(stderr, "Door sensor already initialized\n");
      return -EEXIST;
    }

  memset(&g_door_ctx, 0, sizeof(door_sensor_context_t));

  /* 初始化互斥锁 */

  ret = pthread_mutex_init(&g_door_ctx.mutex, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "Failed to initialize mutex: %d\n", ret);
      return -ret;
    }

  /* 初始化 GPIO */

  ret = gpio_init(DOOR_SENSOR_GPIO_PIN);
  if (ret < 0)
    {
      fprintf(stderr, "Failed to initialize GPIO\n");
      pthread_mutex_destroy(&g_door_ctx.mutex);
      return ret;
    }

  /* 打开 GPIO 值文件用于中断监控 */

  snprintf(path, sizeof(path), GPIO_VALUE_PATH, DOOR_SENSOR_GPIO_PIN);
  g_door_ctx.gpio_fd = open(path, O_RDONLY);
  if (g_door_ctx.gpio_fd < 0)
    {
      perror("Failed to open GPIO value file");
      pthread_mutex_destroy(&g_door_ctx.mutex);
      return -errno;
    }

  /* 读取初始状态 */

  int initial_value = gpio_read_value(DOOR_SENSOR_GPIO_PIN);
  if (initial_value < 0)
    {
      close(g_door_ctx.gpio_fd);
      pthread_mutex_destroy(&g_door_ctx.mutex);
      return initial_value;
    }

  g_door_ctx.status.door_state = initial_value ?
    DOOR_STATE_CLOSED : DOOR_STATE_OPEN;
  g_door_ctx.status.lock_state = LOCK_STATE_UNLOCKED;
  g_door_ctx.status.timestamp = time(NULL);

  /* 创建监控线程 */

  g_door_ctx.running = true;
  ret = pthread_create(&g_door_ctx.monitor_thread, NULL,
                       monitor_thread_func, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "Failed to create monitor thread: %d\n", ret);
      close(g_door_ctx.gpio_fd);
      pthread_mutex_destroy(&g_door_ctx.mutex);
      return -ret;
    }

  g_door_ctx.initialized = true;
  printf("Door sensor initialized successfully\n");
  printf("Initial door state: %s\n",
         g_door_ctx.status.door_state == DOOR_STATE_CLOSED ?
         "CLOSED" : "OPEN");

  return 0;
}

/**
 * @brief 注册状态变化回调函数
 *
 * @param callback 回调函数指针
 * @return 0 成功, 负值失败
 */

int door_sensor_register_callback(door_status_callback_t callback)
{
  if (!g_door_ctx.initialized)
    {
      return -ENODEV;
    }

  pthread_mutex_lock(&g_door_ctx.mutex);
  g_door_ctx.callback = callback;
  pthread_mutex_unlock(&g_door_ctx.mutex);

  return 0;
}

/**
 * @brief 获取当前门状态
 *
 * @param status 输出参数，存储门状态
 * @return 0 成功, 负值失败
 */

int door_sensor_get_status(door_status_t *status)
{
  if (!g_door_ctx.initialized || status == NULL)
    {
      return -EINVAL;
    }

  pthread_mutex_lock(&g_door_ctx.mutex);
  memcpy(status, &g_door_ctx.status, sizeof(door_status_t));
  pthread_mutex_unlock(&g_door_ctx.mutex);

  return 0;
}

/**
 * @brief 设置锁状态
 *
 * @param locked true 锁定, false 解锁
 * @return 0 成功, 负值失败
 */

int door_sensor_set_lock_state(bool locked)
{
  if (!g_door_ctx.initialized)
    {
      return -ENODEV;
    }

  pthread_mutex_lock(&g_door_ctx.mutex);

  g_door_ctx.status.lock_state = locked ?
    LOCK_STATE_LOCKED : LOCK_STATE_UNLOCKED;
  g_door_ctx.status.timestamp = time(NULL);

  /* 通知回调 */
  if (g_door_ctx.callback)
    {
      g_door_ctx.callback(&g_door_ctx.status);
    }

  pthread_mutex_unlock(&g_door_ctx.mutex);

  return 0;
}

/**
 * @brief 反初始化门磁传感器
 *
 * @return 0 成功, 负值失败
 */

int door_sensor_deinit(void)
{
  if (!g_door_ctx.initialized)
    {
      return 0;
    }

  /* 停止监控线程 */
  g_door_ctx.running = false;
  pthread_join(g_door_ctx.monitor_thread, NULL);

  /* 关闭 GPIO */
  close(g_door_ctx.gpio_fd);

  /* 销毁互斥锁 */
  pthread_mutex_destroy(&g_door_ctx.mutex);

  g_door_ctx.initialized = false;
  printf("Door sensor deinitialized\n");

  return 0;
}
