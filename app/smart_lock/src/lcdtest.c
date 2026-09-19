/****************************************************************************
 * app/smart_lock/src/lcdtest.c
 *
 * 智锁卫士 - Team 296 (weihedui)
 *
 * 全屏刷新显示验证工具（诊断用）。
 *
 * 背景：在黄山派（SF32LB52 + CO5300 AMOLED）上实测发现，`fb` 这个官方示例
 * 只有它画的第一个矩形（= 整屏 390x450 纯紫）会出现在屏上，后面 5 个嵌套
 * 矩形全部不可见。串口日志（见 out/fb_debug.txt）证明这 1069 次传输**全部
 * 成功完成、没有一次 `lcd xfer wait timeout`**，所以数据确实送到了 LCDC。
 *
 * 驱动侧的分叉点在 sf32lb_lcd_putarea()
 * （vendor/sifli/boards/sf32lb52/drivers/lcd/sf32lb_lcd.c:458）：
 *
 *   - 全宽（stride == row_bytes，即只有矩形 0）：每批 **24 行** 一次传输
 *   - 局部宽（其余矩形）                      ：**一次只发一行**（y0 == y1）
 *
 * 生产仓库零改动，所以驱动不能补。本工具用来验证"永远整屏刷新"这条绕行
 * 方案是否成立 —— 尤其是**连续多次**全屏刷新（`fb` 只证明了第一次）。
 *
 * 用法：
 *   lcdtest        依次整屏刷 红/绿/蓝/白/黑，每次一格，最后画一幅嵌套图案
 *   lcdtest hold   同上，但最后一步不退出，停在嵌套图案上方便观察
 *
 * 预期：屏幕依次变 红 → 绿 → 蓝 → 白 → 黑 → 彩色嵌套环。
 *       若屏幕停在紫色不再变化，说明连"多次全屏刷新"也不成立。
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <nuttx/video/fb.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LCDTEST_FBDEV "/dev/fb0"

/* RGB565，取自 nuttx/include/nuttx/video/rgbcolors.h */

#define RGB16_BLUE   0x001f
#define RGB16_GREEN  0x07e0
#define RGB16_RED    0xf800
#define RGB16_VIOLET 0xec1d
#define RGB16_ORANGE 0xfd20
#define RGB16_YELLOW 0xffe0
#define RGB16_WHITE  0xffff
#define RGB16_BLACK  0x0000

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int      g_fd = -1;
static uint8_t *g_fbmem;
static struct fb_videoinfo_s g_vinfo;
static struct fb_planeinfo_s g_pinfo;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* 把整屏填成一种颜色（只写内存，不刷屏） */

static void fill_all(uint16_t color)
{
  uint8_t *row = g_fbmem;
  int y;
  int x;

  for (y = 0; y < g_vinfo.yres; y++)
    {
      uint16_t *p = (uint16_t *)row;

      for (x = 0; x < g_vinfo.xres; x++)
        {
          *p++ = color;
        }

      row += g_pinfo.stride;
    }
}

/* 只写内存里的一个矩形，不刷屏 */

static void fill_rect(int ax, int ay, int w, int h, uint16_t color)
{
  int y;
  int x;

  for (y = ay; y < ay + h && y < g_vinfo.yres; y++)
    {
      uint16_t *p = (uint16_t *)(g_fbmem + (size_t)y * g_pinfo.stride) + ax;

      for (x = 0; x < w && ax + x < g_vinfo.xres; x++)
        {
          *p++ = color;
        }
    }
}

/* 整屏刷新一次 —— 这是本工具唯一使用的刷新方式 */

static int flush_full(void)
{
  struct fb_area_s area;
  int ret;

  area.x = 0;
  area.y = 0;
  area.w = g_vinfo.xres;
  area.h = g_vinfo.yres;

  ret = ioctl(g_fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));
  if (ret < 0)
    {
      printf("  !! FBIO_UPDATE FULL failed, errno=%d\n", errno);
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int lcdtest_main(int argc, char *argv[])
{
  static const uint16_t colors[] =
  {
    RGB16_RED, RGB16_GREEN, RGB16_BLUE, RGB16_WHITE, RGB16_BLACK
  };

  static const char *names[] =
  {
    "RED", "GREEN", "BLUE", "WHITE", "BLACK"
  };

  int hold = 0;
  int i;
  int ret;

  if (argc > 1 && strcmp(argv[1], "hold") == 0)
    {
      hold = 1;
    }

  g_fd = open(LCDTEST_FBDEV, O_RDWR);
  if (g_fd < 0)
    {
      printf("lcdtest: open %s failed: %d\n", LCDTEST_FBDEV, errno);
      return EXIT_FAILURE;
    }

  if (ioctl(g_fd, FBIOGET_VIDEOINFO,
            (unsigned long)((uintptr_t)&g_vinfo)) < 0)
    {
      printf("lcdtest: FBIOGET_VIDEOINFO failed: %d\n", errno);
      close(g_fd);
      return EXIT_FAILURE;
    }

  if (ioctl(g_fd, FBIOGET_PLANEINFO,
            (unsigned long)((uintptr_t)&g_pinfo)) < 0)
    {
      printf("lcdtest: FBIOGET_PLANEINFO failed: %d\n", errno);
      close(g_fd);
      return EXIT_FAILURE;
    }

  printf("lcdtest: xres=%d yres=%d bpp=%d stride=%d fblen=%lu\n",
         g_vinfo.xres, g_vinfo.yres, g_pinfo.bpp,
         g_pinfo.stride, (unsigned long)g_pinfo.fblen);

  if (g_pinfo.bpp != 16)
    {
      printf("lcdtest: only 16bpp supported, got %d\n", g_pinfo.bpp);
      close(g_fd);
      return EXIT_FAILURE;
    }

  g_fbmem = mmap(NULL, g_pinfo.fblen, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FILE, g_fd, 0);
  if (g_fbmem == MAP_FAILED)
    {
      printf("lcdtest: mmap failed: %d\n", errno);
      close(g_fd);
      return EXIT_FAILURE;
    }

  printf("lcdtest: mapped fb at %p\n", g_fbmem);
  printf("lcdtest: 请盯着屏幕 —— 应依次看到 红 绿 蓝 白 黑 彩色嵌套环\n");

  /* 连续多次整屏刷新：验证"多次全屏更新"是否成立 */

  for (i = 0; i < 5; i++)
    {
      fill_all(colors[i]);
      printf("step %d: full screen %-5s (0x%04x) ... ", i + 1, names[i],
             colors[i]);
      ret = flush_full();
      printf("FBIO_UPDATE ret=%d\n", ret);

      usleep(2 * 1000 * 1000);
    }

  /* 最后一步：把 fb 那套嵌套图案画进内存，**只做一次整屏刷新**。
   * 如果这一步能在屏上看到完整的嵌套环，说明整屏刷新能把任意内容正确
   * 送上屏 —— 产品就可以完全建立在这条路径上。
   */

  {
    static const uint16_t palette[6] =
    {
      RGB16_VIOLET, RGB16_BLUE, RGB16_GREEN,
      RGB16_YELLOW, RGB16_ORANGE, RGB16_RED
    };

    int xstep = g_vinfo.xres / 11;
    int ystep = g_vinfo.yres / 11;
    int w = g_vinfo.xres;
    int h = g_vinfo.yres;
    int x = 0;
    int y = 0;
    int c;

    fill_all(RGB16_BLACK);

    for (c = 0; c < 6; c++)
      {
        printf("step 6.%d: nested rect %d at (%d,%d) %dx%d, color 0x%04x\n",
               c + 1, c, x, y, w, h, palette[c]);
        fill_rect(x, y, w, h, palette[c]);

        x += xstep;
        y += ystep;
        w -= 2 * xstep;
        h -= 2 * ystep;
      }

    printf("step 7: ONE full-screen update of the nested pattern ... ");
    ret = flush_full();
    printf("FBIO_UPDATE ret=%d\n", ret);
  }

  printf("lcdtest: done. 期望屏上是 紫边+蓝绿黄橙环+红心\n");

  if (hold)
    {
      printf("lcdtest: hold 模式，停在当前画面 60 秒\n");
      usleep(60 * 1000 * 1000);
    }

  munmap(g_fbmem, g_pinfo.fblen);
  close(g_fd);
  return EXIT_SUCCESS;
}
