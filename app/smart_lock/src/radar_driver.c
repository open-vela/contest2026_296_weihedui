/****************************************************************************
 * app/smart_lock/src/radar_driver.c
 *
 * HLK-LD2410 毫米波雷达驱动实现
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change History:
 *   Date        Author   Description
 *   2026-09-12  MemberB  Initial version - UART frame parsing implementation
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
#include <termios.h>
#include <time.h>

#include "radar_driver.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UART_DEVICE_PATH    "/dev/ttyS1"  /* UART 设备路径 */
#define UART_BAUD_RATE      B115200       /* 波特率 */
#define RECEIVE_THREAD_STACK_SIZE  4096   /* 接收线程栈大小 */
#define RECEIVE_TIMEOUT_MS   100          /* 接收超时时间 */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* 驱动上下文结构体 */

typedef struct
{
  int              uart_fd;          /* UART 文件描述符 */
  bool             initialized;      /* 初始化标志 */
  bool             running;          /* 接收线程运行标志 */
  pthread_t        receive_thread;   /* 接收线程 */
  pthread_mutex_t  mutex;            /* 互斥锁 */
  ring_buffer_t    ring_buffer;      /* 环形缓冲区 */
  radar_result_t   last_result;      /* 最近一次解析结果 */
  radar_callback_t callback;         /* 回调函数 */
  parse_state_t    parse_state;      /* 帧解析状态机状态 */
} radar_context_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  uart_init(const char *device, speed_t baud_rate);
static void *receive_thread_func(void *arg);
static int  ring_buffer_put(ring_buffer_t *rb, const uint8_t *data, size_t len);
static int  ring_buffer_get(ring_buffer_t *rb, uint8_t *data, size_t len);
static int  parse_frame_from_buffer(radar_context_t *ctx);
static uint8_t calculate_checksum(const uint8_t *data, size_t len);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static radar_context_t g_radar_ctx;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 初始化 UART 接口
 *
 * @param device 设备路径
 * @param baud_rate 波特率
 * @return 文件描述符, 负值表示失败
 */

static int uart_init(const char *device, speed_t baud_rate)
{
  int fd;
  struct termios tty;

  fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0)
    {
      perror("Failed to open UART device");
      return -errno;
    }

  memset(&tty, 0, sizeof(tty));

  if (tcgetattr(fd, &tty) != 0)
    {
      perror("Failed to get UART attributes");
      close(fd);
      return -errno;
    }

  /* 配置 UART 参数: 115200 baud, 8N1 */

  cfsetospeed(&tty, baud_rate);
  cfsetispeed(&tty, baud_rate);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  /* 8 数据位 */
  tty.c_cflag &= ~PARENB;                        /* 无校验 */
  tty.c_cflag &= ~CSTOPB;                        /* 1 停止位 */
  tty.c_cflag &= ~CRTSCTS;                       /* 无硬件流控 */
  tty.c_cflag |= CREAD | CLOCAL;                 /* 使能接收 */

  tty.c_iflag &= ~(IXON | IXOFF | IXANY);       /* 无软件流控 */
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
                    ISTRIP | INLCR | IGNCR | ICRNL);

  tty.c_lflag &= ~(ECHO | ECHONL | ICANON |
                    ISIG | IEXTEN);

  tty.c_cc[VMIN]  = 0;  /* 非阻塞读取 */
  tty.c_cc[VTIME] = 1;  /* 0.1 秒超时 */

  if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
      perror("Failed to set UART attributes");
      close(fd);
      return -errno;
    }

  return fd;
}

/**
 * @brief 环形缓冲区写入数据
 *
 * @param rb 环形缓冲区指针
 * @param data 数据指针
 * @param len 数据长度
 * @return 写入的字节数
 */

static int ring_buffer_put(ring_buffer_t *rb, const uint8_t *data, size_t len)
{
  size_t i;
  size_t free_space;

  free_space = RADAR_BUFFER_SIZE - rb->count;
  if (len > free_space)
    {
      len = free_space;
    }

  for (i = 0; i < len; i++)
    {
      rb->buffer[rb->head] = data[i];
      rb->head = (rb->head + 1) % RADAR_BUFFER_SIZE;
    }

  rb->count += len;
  return (int)len;
}

/**
 * @brief 环形缓冲区读取数据
 *
 * @param rb 环形缓冲区指针
 * @param data 输出数据指针
 * @param len 请求读取长度
 * @return 实际读取的字节数
 */

static int ring_buffer_get(ring_buffer_t *rb, uint8_t *data, size_t len)
{
  size_t i;

  if (len > rb->count)
    {
      len = rb->count;
    }

  for (i = 0; i < len; i++)
    {
      data[i] = rb->buffer[rb->tail];
      rb->tail = (rb->tail + 1) % RADAR_BUFFER_SIZE;
    }

  rb->count -= len;
  return (int)len;
}

/**
 * @brief 计算校验和
 *
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和
 */

static uint8_t calculate_checksum(const uint8_t *data, size_t len)
{
  uint8_t checksum = 0;
  size_t i;

  for (i = 0; i < len; i++)
    {
      checksum += data[i];
    }

  return checksum;
}

/**
 * @brief 从缓冲区解析数据帧
 *
 * @param ctx 驱动上下文指针
 * @return 0 成功, 负值失败
 */

static int parse_frame_from_buffer(radar_context_t *ctx)
{
  uint8_t byte;
  static uint8_t frame_buffer[256];
  static size_t frame_index = 0;
  static uint16_t data_len = 0;
  static uint8_t func_code = 0;

  while (ring_buffer_get(&ctx->ring_buffer, &byte, 1) > 0)
    {
      switch (ctx->parse_state)
        {
          case PARSE_STATE_IDLE:
            /* 等待帧头 0xAA */
            if (byte == RADAR_FRAME_HEADER)
              {
                frame_index = 0;
                frame_buffer[frame_index++] = byte;
                ctx->parse_state = PARSE_STATE_LENGTH;
              }
            break;

          case PARSE_STATE_LENGTH:
            /* 解析数据长度 (2 字节, 小端序) */
            frame_buffer[frame_index++] = byte;
            if (frame_index == 3)
              {
                data_len = (frame_buffer[2] << 8) | frame_buffer[1];
                if (data_len > 250)  /* 长度异常 */
                  {
                    ctx->parse_state = PARSE_STATE_IDLE;
                    frame_index = 0;
                  }
                else
                  {
                    ctx->parse_state = PARSE_STATE_FUNC_CODE;
                  }
              }
            break;

          case PARSE_STATE_FUNC_CODE:
            /* 解析功能码 */
            frame_buffer[frame_index++] = byte;
            func_code = byte;
            ctx->parse_state = PARSE_STATE_DATA;
            break;

          case PARSE_STATE_DATA:
            /* 接收数据区 */
            frame_buffer[frame_index++] = byte;
            if (frame_index >= 3 + 1 + data_len)
              {
                ctx->parse_state = PARSE_STATE_CHECKSUM;
              }
            break;

          case PARSE_STATE_CHECKSUM:
            /* 校验和验证 */
            frame_buffer[frame_index++] = byte;
            {
              uint8_t calc_checksum = calculate_checksum(
                frame_buffer, frame_index - 2);

              if (calc_checksum == byte)
                {
                  ctx->parse_state = PARSE_STATE_TAIL;
                }
              else
                {
                  /* 校验和错误，重新同步 */
                  ctx->parse_state = PARSE_STATE_IDLE;
                  frame_index = 0;
                  return -1;
                }
            }
            break;

          case PARSE_STATE_TAIL:
            /* 验证帧尾 */
            if (byte == RADAR_FRAME_TAIL)
              {
                /* 帧解析成功，提取数据 */
                switch (func_code)
                  {
                    case RADAR_FUNC_TARGET_EXIST:
                      ctx->last_result.target_detected =
                        (frame_buffer[4] != 0);
                      break;

                    case RADAR_FUNC_MOTION_STATE:
                      ctx->last_result.motion_state = frame_buffer[4];
                      break;

                    case RADAR_FUNC_REGION_INFO:
                      if (data_len >= 4)
                        {
                          ctx->last_result.target_distance =
                            (frame_buffer[6] << 8) | frame_buffer[5];
                          ctx->last_result.energy_value = frame_buffer[7];
                        }
                      break;
                  }

                ctx->last_result.timestamp = (uint32_t)time(NULL);

                /* 通知回调 */
                if (ctx->callback)
                  {
                    ctx->callback(&ctx->last_result);
                  }

                ctx->parse_state = PARSE_STATE_IDLE;
                frame_index = 0;
                return 0;
              }
            else
              {
                /* 帧尾错误，重新同步 */
                ctx->parse_state = PARSE_STATE_IDLE;
                frame_index = 0;
                return -1;
              }
            break;
        }
    }

  return -1;  /* 数据不完整 */
}

/**
 * @brief UART 接收线程函数
 *
 * @param arg 线程参数 (radar_context_t 指针)
 * @return NULL
 */

static void *receive_thread_func(void *arg)
{
  radar_context_t *ctx = (radar_context_t *)arg;
  uint8_t buffer[256];
  ssize_t bytes_read;

  while (ctx->running)
    {
      bytes_read = read(ctx->uart_fd, buffer, sizeof(buffer));

      if (bytes_read > 0)
        {
          pthread_mutex_lock(&ctx->mutex);
          ring_buffer_put(&ctx->ring_buffer, buffer, bytes_read);
          parse_frame_from_buffer(ctx);
          pthread_mutex_unlock(&ctx->mutex);
        }
      else if (bytes_read < 0 && errno != EAGAIN)
        {
          perror("UART read error");
          break;
        }

      usleep(10000);  /* 10ms 延迟 */
    }

  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * @brief 初始化雷达驱动
 *
 * @return 0 成功, 负值失败
 */

int radar_init(void)
{
  int ret;

  if (g_radar_ctx.initialized)
    {
      fprintf(stderr, "Radar driver already initialized\n");
      return -EEXIST;
    }

  memset(&g_radar_ctx, 0, sizeof(radar_context_t));

  /* 初始化互斥锁 */

  ret = pthread_mutex_init(&g_radar_ctx.mutex, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "Failed to initialize mutex: %d\n", ret);
      return -ret;
    }

  /* 初始化 UART */

  g_radar_ctx.uart_fd = uart_init(UART_DEVICE_PATH, UART_BAUD_RATE);
  if (g_radar_ctx.uart_fd < 0)
    {
      fprintf(stderr, "Failed to initialize UART\n");
      pthread_mutex_destroy(&g_radar_ctx.mutex);
      return g_radar_ctx.uart_fd;
    }

  /* 初始化解析状态 */

  g_radar_ctx.parse_state = PARSE_STATE_IDLE;

  /* 创建接收线程 */

  g_radar_ctx.running = true;
  ret = pthread_create(&g_radar_ctx.receive_thread, NULL,
                       receive_thread_func, &g_radar_ctx);
  if (ret != 0)
    {
      fprintf(stderr, "Failed to create receive thread: %d\n", ret);
      close(g_radar_ctx.uart_fd);
      pthread_mutex_destroy(&g_radar_ctx.mutex);
      return -ret;
    }

  g_radar_ctx.initialized = true;
  printf("Radar driver initialized successfully\n");

  return 0;
}

/**
 * @brief 注册数据回调函数
 *
 * @param callback 回调函数指针
 * @return 0 成功, 负值失败
 */

int radar_register_callback(radar_callback_t callback)
{
  if (!g_radar_ctx.initialized)
    {
      return -ENODEV;
    }

  pthread_mutex_lock(&g_radar_ctx.mutex);
  g_radar_ctx.callback = callback;
  pthread_mutex_unlock(&g_radar_ctx.mutex);

  return 0;
}

/**
 * @brief 获取当前雷达状态
 *
 * @param result 输出参数，存储雷达数据
 * @return 0 成功, 负值失败
 */

int radar_get_status(radar_result_t *result)
{
  if (!g_radar_ctx.initialized || result == NULL)
    {
      return -EINVAL;
    }

  pthread_mutex_lock(&g_radar_ctx.mutex);
  memcpy(result, &g_radar_ctx.last_result, sizeof(radar_result_t));
  pthread_mutex_unlock(&g_radar_ctx.mutex);

  return 0;
}

/**
 * @brief 解析雷达数据帧
 *
 * @param frame 帧数据指针
 * @param len 帧数据长度
 * @return 0 成功, 负值失败
 */

int radar_parse_frame(const uint8_t *frame, size_t len)
{
  if (frame == NULL || len < RADAR_FRAME_MIN_LEN)
    {
      return -EINVAL;
    }

  /* 验证帧头 */
  if (frame[0] != RADAR_FRAME_HEADER)
    {
      return -EINVAL;
    }

  /* 验证帧尾 */
  if (frame[len - 1] != RADAR_FRAME_TAIL)
    {
      return -EINVAL;
    }

  /* 解析数据长度 */
  uint16_t data_len = (frame[2] << 8) | frame[1];

  /* 验证长度 */
  if (len != (size_t)(4 + data_len + 2))  /* 头 + 长度 + 功能码 + 数据 + 校验 + 尾 */
    {
      return -EINVAL;
    }

  /* 验证校验和 */
  uint8_t calc_checksum = calculate_checksum(frame, len - 2);
  if (calc_checksum != frame[len - 2])
    {
      return -EINVAL;
    }

  /* 解析功能码和数据 */
  uint8_t func_code = frame[3];

  pthread_mutex_lock(&g_radar_ctx.mutex);

  switch (func_code)
    {
      case RADAR_FUNC_TARGET_EXIST:
        g_radar_ctx.last_result.target_detected = (frame[4] != 0);
        break;

      case RADAR_FUNC_MOTION_STATE:
        g_radar_ctx.last_result.motion_state = frame[4];
        break;

      case RADAR_FUNC_REGION_INFO:
        if (data_len >= 4)
          {
            g_radar_ctx.last_result.target_distance =
              (frame[6] << 8) | frame[5];
            g_radar_ctx.last_result.energy_value = frame[7];
          }
        break;

      default:
        pthread_mutex_unlock(&g_radar_ctx.mutex);
        return -EINVAL;
    }

  g_radar_ctx.last_result.timestamp = (uint32_t)time(NULL);

  /* 通知回调 */
  if (g_radar_ctx.callback)
    {
      g_radar_ctx.callback(&g_radar_ctx.last_result);
    }

  pthread_mutex_unlock(&g_radar_ctx.mutex);

  return 0;
}

/**
 * @brief 反初始化雷达驱动
 *
 * @return 0 成功, 负值失败
 */

int radar_deinit(void)
{
  if (!g_radar_ctx.initialized)
    {
      return 0;
    }

  /* 停止接收线程 */
  g_radar_ctx.running = false;
  pthread_join(g_radar_ctx.receive_thread, NULL);

  /* 关闭 UART */
  close(g_radar_ctx.uart_fd);

  /* 销毁互斥锁 */
  pthread_mutex_destroy(&g_radar_ctx.mutex);

  g_radar_ctx.initialized = false;
  printf("Radar driver deinitialized\n");

  return 0;
}
