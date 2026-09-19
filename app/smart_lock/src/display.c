/****************************************************************************
 * app/smart_lock/src/display.c
 *
 * 智锁卫士 - AMOLED 状态显示模块
 *
 * 把门锁的实时状态渲染到黄山派板载的 1.85" 390x450 AMOLED（CO5300, QSPI）上。
 *
 * ---------------------------------------------------------------------------
 * 为什么每帧都整屏重画
 * ---------------------------------------------------------------------------
 * 实机实测（见 src/lcdtest.c 头部说明与 docs）表明：这块屏上**只有整屏
 * 更新能真正上屏**。驱动 sf32lb_lcd_putarea()
 * （vendor/sifli/boards/sf32lb52/drivers/lcd/sf32lb_lcd.c:458）按
 * 「stride 是否等于整行字节数」分成两条路：
 *
 *   - 全宽 → 每批 24 行一次传输  → 有效
 *   - 局部宽 → 一次只发一行      → 静默失效（返回成功，屏上无变化）
 *
 * 所以本模块不使用任何局部区域刷新：先在帧缓冲里把整幅画面画好，
 * 再用一次 FBIO_UPDATE 覆盖 (0,0)-(389,449)。整屏一次约 19 批 ≈ 0.35 s，
 * 即约 3 fps 的上限，因此刷新周期取 1 Hz。
 *
 * 驱动在工作在只读的生产仓库里，不能改；本模块不需要改它。
 *
 * ---------------------------------------------------------------------------
 * 依赖
 * ---------------------------------------------------------------------------
 * 只用各模块已有的公开 getter，不修改任何其他模块：
 *   door_sensor_get_status() / radar_get_status() / motor_get_state() /
 *   ble_is_connected() / safety_is_anti_pinch_triggered() /
 *   get_auto_close_timeout()
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
#include <time.h>
#include <pthread.h>

#include <nuttx/video/fb.h>

#include "smart_lock.h"
#include "display.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DISPLAY_FBDEV       "/dev/fb0"

/* 整屏更新一次约 0.35 s，1 Hz 刷新留足余量 */

#define DISPLAY_PERIOD_S    1

/* 即使状态没变，也每 10 秒重推一帧。
 * 板载 lyra demo 应用持有 LVGL，理论上可能覆盖我们的画面，心跳帧用来兜底。
 */

#define DISPLAY_HEARTBEAT_S 10

/* 画面尺寸（与面板一致） */

#define SCREEN_W            390
#define SCREEN_H            450

/* 字体：5x7 点阵，列优先，bit0 为最上面一行 */

#define FONT_FIRST          0x20
#define FONT_LAST           0x5f
#define FONT_W              5
#define FONT_H              7
#define FONT_ADV            6   /* 含 1 列字间距 */
#define FONT_LINE           8   /* 含 1 行行间距 */

/* 配色（RGB565） */

#define C_BG      0x0000        /* 背景：黑 */
#define C_TEXT    0xffff        /* 正文：白 */
#define C_DIM     0x8410        /* 次要信息：灰 */
#define C_LINE    0x4208        /* 分隔线：暗灰 */
#define C_GREEN   0x07e0        /* 安全 / 已锁定 */
#define C_AMBER   0xfd20        /* 注意 / 未锁定 */
#define C_RED     0xf800        /* 告警 */
#define C_CYAN    0x07ff        /* 信息 */
#define C_BLUE    0x001f

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 5x7 点阵字库，覆盖可打印 ASCII 0x20-0x5F（大写字母、数字、常用符号）。
 * 小写字母在 draw_text() 里统一转成大写，因此不需要小写字模。 */

static const uint8_t g_font[FONT_LAST - FONT_FIRST + 1][FONT_W] =
{
  { 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ' ' */
  { 0x00, 0x00, 0x5f, 0x00, 0x00 }, /* '!' */
  { 0x00, 0x07, 0x00, 0x07, 0x00 }, /* '"' */
  { 0x14, 0x7f, 0x14, 0x7f, 0x14 }, /* '#' */
  { 0x24, 0x2a, 0x7f, 0x2a, 0x12 }, /* '$' */
  { 0x23, 0x13, 0x08, 0x64, 0x62 }, /* '%' */
  { 0x36, 0x49, 0x55, 0x22, 0x50 }, /* '&' */
  { 0x00, 0x05, 0x03, 0x00, 0x00 }, /* ''' */
  { 0x00, 0x1c, 0x22, 0x41, 0x00 }, /* '(' */
  { 0x00, 0x41, 0x22, 0x1c, 0x00 }, /* ')' */
  { 0x14, 0x08, 0x3e, 0x08, 0x14 }, /* '*' */
  { 0x08, 0x08, 0x3e, 0x08, 0x08 }, /* '+' */
  { 0x00, 0x50, 0x30, 0x00, 0x00 }, /* ',' */
  { 0x08, 0x08, 0x08, 0x08, 0x08 }, /* '-' */
  { 0x00, 0x60, 0x60, 0x00, 0x00 }, /* '.' */
  { 0x20, 0x10, 0x08, 0x04, 0x02 }, /* '/' */
  { 0x3e, 0x51, 0x49, 0x45, 0x3e }, /* '0' */
  { 0x00, 0x42, 0x7f, 0x40, 0x00 }, /* '1' */
  { 0x42, 0x61, 0x51, 0x49, 0x46 }, /* '2' */
  { 0x21, 0x41, 0x45, 0x4b, 0x31 }, /* '3' */
  { 0x18, 0x14, 0x12, 0x7f, 0x10 }, /* '4' */
  { 0x27, 0x45, 0x45, 0x45, 0x39 }, /* '5' */
  { 0x3c, 0x4a, 0x49, 0x49, 0x30 }, /* '6' */
  { 0x01, 0x71, 0x09, 0x05, 0x03 }, /* '7' */
  { 0x36, 0x49, 0x49, 0x49, 0x36 }, /* '8' */
  { 0x06, 0x49, 0x49, 0x29, 0x1e }, /* '9' */
  { 0x00, 0x36, 0x36, 0x00, 0x00 }, /* ':' */
  { 0x00, 0x56, 0x36, 0x00, 0x00 }, /* ';' */
  { 0x08, 0x14, 0x22, 0x41, 0x00 }, /* '<' */
  { 0x14, 0x14, 0x14, 0x14, 0x14 }, /* '=' */
  { 0x00, 0x41, 0x22, 0x14, 0x08 }, /* '>' */
  { 0x02, 0x01, 0x51, 0x09, 0x06 }, /* '?' */
  { 0x32, 0x49, 0x79, 0x41, 0x3e }, /* '@' */
  { 0x7e, 0x11, 0x11, 0x11, 0x7e }, /* 'A' */
  { 0x7f, 0x49, 0x49, 0x49, 0x36 }, /* 'B' */
  { 0x3e, 0x41, 0x41, 0x41, 0x22 }, /* 'C' */
  { 0x7f, 0x41, 0x41, 0x22, 0x1c }, /* 'D' */
  { 0x7f, 0x49, 0x49, 0x49, 0x41 }, /* 'E' */
  { 0x7f, 0x09, 0x09, 0x09, 0x01 }, /* 'F' */
  { 0x3e, 0x41, 0x49, 0x49, 0x7a }, /* 'G' */
  { 0x7f, 0x08, 0x08, 0x08, 0x7f }, /* 'H' */
  { 0x00, 0x41, 0x7f, 0x41, 0x00 }, /* 'I' */
  { 0x20, 0x40, 0x41, 0x3f, 0x01 }, /* 'J' */
  { 0x7f, 0x08, 0x14, 0x22, 0x41 }, /* 'K' */
  { 0x7f, 0x40, 0x40, 0x40, 0x40 }, /* 'L' */
  { 0x7f, 0x02, 0x0c, 0x02, 0x7f }, /* 'M' */
  { 0x7f, 0x04, 0x08, 0x10, 0x7f }, /* 'N' */
  { 0x3e, 0x41, 0x41, 0x41, 0x3e }, /* 'O' */
  { 0x7f, 0x09, 0x09, 0x09, 0x06 }, /* 'P' */
  { 0x3e, 0x41, 0x51, 0x21, 0x5e }, /* 'Q' */
  { 0x7f, 0x09, 0x19, 0x29, 0x46 }, /* 'R' */
  { 0x46, 0x49, 0x49, 0x49, 0x31 }, /* 'S' */
  { 0x01, 0x01, 0x7f, 0x01, 0x01 }, /* 'T' */
  { 0x3f, 0x40, 0x40, 0x40, 0x3f }, /* 'U' */
  { 0x1f, 0x20, 0x40, 0x20, 0x1f }, /* 'V' */
  { 0x3f, 0x40, 0x38, 0x40, 0x3f }, /* 'W' */
  { 0x63, 0x14, 0x08, 0x14, 0x63 }, /* 'X' */
  { 0x07, 0x08, 0x70, 0x08, 0x07 }, /* 'Y' */
  { 0x61, 0x51, 0x49, 0x45, 0x43 }, /* 'Z' */
  { 0x00, 0x7f, 0x41, 0x41, 0x00 }, /* '[' */
  { 0x02, 0x04, 0x08, 0x10, 0x20 }, /* '\' */
  { 0x00, 0x41, 0x41, 0x7f, 0x00 }, /* ']' */
  { 0x04, 0x02, 0x01, 0x02, 0x04 }, /* '^' */
  { 0x40, 0x40, 0x40, 0x40, 0x40 }, /* '_' */
};

static int      g_fd = -1;
static uint8_t *g_fb;
static struct fb_videoinfo_s g_vinfo;
static struct fb_planeinfo_s g_pinfo;
static bool     g_ready;

static pthread_t g_thread;
static bool      g_thread_started;

/* 上一次渲染用到的状态快照，用来判断要不要重推一帧 */

typedef struct
{
  bool     valid;
  int      door;
  int      lock;
  int      motor;
  int      radar;
  uint16_t distance;
  bool     ble;
  bool     pinch;
  int      autoclose;
} display_snapshot_t;

static display_snapshot_t g_prev;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* 往帧缓冲写一个像素。越界直接丢弃，调用方不必自己裁剪。 */

static void put_px(int x, int y, uint16_t color)
{
  if (x < 0 || y < 0 || x >= g_vinfo.xres || y >= g_vinfo.yres)
    {
      return;
    }

  *(uint16_t *)(g_fb + (size_t)y * g_pinfo.stride + (size_t)x * 2) = color;
}

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
  int i;
  int j;

  for (j = y; j < y + h; j++)
    {
      for (i = x; i < x + w; i++)
        {
          put_px(i, j, color);
        }
    }
}

static void fill_screen(uint16_t color)
{
  fill_rect(0, 0, g_vinfo.xres, g_vinfo.yres, color);
}

/* 画一个字符。bg 用来清掉字模周围的底色，避免残影。 */

static void draw_char(int x, int y, int scale, char ch,
                      uint16_t fg, uint16_t bg)
{
  const uint8_t *glyph;
  int col;
  int row;

  if (ch >= 'a' && ch <= 'z')
    {
      ch = (char)(ch - 'a' + 'A');
    }

  if (ch < FONT_FIRST || ch > FONT_LAST)
    {
      ch = '?';
    }

  glyph = g_font[(int)ch - FONT_FIRST];

  fill_rect(x, y, FONT_ADV * scale, FONT_LINE * scale, bg);

  for (col = 0; col < FONT_W; col++)
    {
      uint8_t bits = glyph[col];

      for (row = 0; row < FONT_H; row++)
        {
          if (bits & (1 << row))
            {
              fill_rect(x + col * scale, y + row * scale, scale, scale, fg);
            }
        }
    }
}

static int text_width(int scale, const char *s)
{
  int n = (int)strlen(s);

  if (n == 0)
    {
      return 0;
    }

  /* 最后一个字符不含字间距 */

  return (n * FONT_ADV - 1) * scale;
}

static void draw_text(int x, int y, int scale, const char *s,
                      uint16_t fg, uint16_t bg)
{
  while (*s)
    {
      draw_char(x, y, scale, *s, fg, bg);
      x += FONT_ADV * scale;
      s++;
    }
}

static void draw_text_centered(int y, int scale, const char *s,
                               uint16_t fg, uint16_t bg)
{
  int x = (g_vinfo.xres - text_width(scale, s)) / 2;

  if (x < 0)
    {
      x = 0;
    }

  draw_text(x, y, scale, s, fg, bg);
}

/* 一行「标签 + 值」：标签灰色左对齐，值彩色右对齐。 */

static void draw_field(int y, int scale, const char *label, const char *value,
                       uint16_t value_color, uint16_t bg)
{
  int vx = g_vinfo.xres - text_width(scale, value) - 6 * scale;

  draw_text(6 * scale, y, scale, label, C_DIM, bg);

  if (vx < 0)
    {
      vx = 0;
    }

  draw_text(vx, y, scale, value, value_color, bg);
}

static const char *door_to_string(door_state_t s)
{
  switch (s)
    {
      case DOOR_STATE_OPEN:   return "OPEN";
      case DOOR_STATE_CLOSED: return "CLOSED";
      default:                return "UNKNOWN";
    }
}

static const char *radar_to_string(radar_state_t s)
{
  switch (s)
    {
      case RADAR_STATE_MOVING:     return "MOVING";
      case RADAR_STATE_STATIONARY: return "STILL";
      default:                     return "NONE";
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* 采样一次全部状态 */

static void display_sample(display_snapshot_t *s)
{
  door_status_t  door;
  radar_result_t radar;

  memset(s, 0, sizeof(*s));

  if (door_sensor_get_status(&door) == 0)
    {
      s->door = (int)door.door_state;
      s->lock = (int)door.lock_state;
    }
  else
    {
      s->door = (int)DOOR_STATE_UNKNOWN;
      s->lock = (int)LOCK_STATE_UNKNOWN;
    }

  s->motor = (int)motor_get_state();

  if (radar_get_status(&radar) == 0)
    {
      s->radar    = (int)radar.state;
      s->distance = radar.distance;
    }
  else
    {
      s->radar    = (int)RADAR_STATE_NO_TARGET;
      s->distance = 0;
    }

  s->ble       = ble_is_connected();
  s->pinch     = safety_is_anti_pinch_triggered();
  s->autoclose = get_auto_close_timeout();
  s->valid     = true;
}

static bool display_changed(const display_snapshot_t *a,
                            const display_snapshot_t *b)
{
  return a->door      != b->door      ||
         a->lock      != b->lock      ||
         a->motor     != b->motor     ||
         a->radar     != b->radar     ||
         a->distance  != b->distance  ||
         a->ble       != b->ble       ||
         a->pinch     != b->pinch     ||
         a->autoclose != b->autoclose;
}

/* 把状态画进帧缓冲（不推屏） */

static void display_render(const display_snapshot_t *s)
{
  const char *title;
  const char *banner;
  const char *banner_sub;
  uint16_t    accent;
  char        buf[32];
  int         y;

  /* 顶栏配色由最高优先级的状态决定：防夹 > 锁状态 */

  if (s->pinch)
    {
      title        = "ALARM";
      banner       = "ANTI-PINCH";
      banner_sub   = "MOTOR STOPPED";
      accent       = C_RED;
    }
  else if (s->lock == (int)LOCK_STATE_LOCKED)
    {
      title        = "LOCKED";
      banner       = "SECURE";
      banner_sub   = "ALL SENSORS OK";
      accent       = C_GREEN;
    }
  else if (s->lock == (int)LOCK_STATE_UNLOCKED)
    {
      title        = "UNLOCKED";
      banner       = "UNSECURED";
      banner_sub   = "DOOR MAY BE OPEN";
      accent       = C_AMBER;
    }
  else
    {
      title        = "NO DATA";
      banner       = "SENSOR";
      banner_sub   = "NOT AVAILABLE";
      accent       = C_DIM;
    }

  fill_screen(C_BG);

  /* 标题栏 */

  draw_text_centered(8, 2, "TEAM 296  SMART LOCK GUARDIAN", C_DIM, C_BG);

  /* 大号锁状态 */

  draw_text_centered(34, 6, title, accent, C_BG);

  /* 分隔线 */

  fill_rect(0, 90, g_vinfo.xres, 3, accent);

  /* 状态明细 */

  y = 108;
  draw_field(y, 3, "DOOR", door_to_string((door_state_t)s->door),
             C_TEXT, C_BG);

  y += 30;
  draw_field(y, 3, "MOTOR", motor_state_to_string((motor_state_t)s->motor),
             C_TEXT, C_BG);

  y += 30;
  draw_field(y, 3, "RADAR", radar_to_string((radar_state_t)s->radar),
             s->radar == (int)RADAR_STATE_NO_TARGET ? C_DIM : C_CYAN, C_BG);

  y += 30;
  if (s->distance > 0)
    {
      snprintf(buf, sizeof(buf), "%u CM", (unsigned)s->distance);
    }
  else
    {
      snprintf(buf, sizeof(buf), "-- CM");
    }

  draw_field(y, 3, "DIST", buf, C_CYAN, C_BG);

  y += 30;
  draw_field(y, 3, "BLE", s->ble ? "CONNECTED" : "OFF",
             s->ble ? C_GREEN : C_DIM, C_BG);

  y += 30;
  snprintf(buf, sizeof(buf), "%d S", s->autoclose);
  draw_field(y, 3, "AUTOCLOSE", buf, C_TEXT, C_BG);

  y += 30;
  draw_field(y, 3, "PINCH", s->pinch ? "TRIGGERED" : "SAFE",
             s->pinch ? C_RED : C_GREEN, C_BG);

  /* 底部横幅 */

  fill_rect(0, 340, g_vinfo.xres, g_vinfo.yres - 340, accent);
  draw_text_centered(362, 5, banner, C_BG, accent);
  draw_text_centered(410, 2, banner_sub, C_BG, accent);
}

/* 一次覆盖全屏的 FBIO_UPDATE —— 唯一可靠的推屏方式 */

static int display_push(void)
{
  struct fb_area_s area;
  int ret;

  area.x = 0;
  area.y = 0;
  area.w = (fb_coord_t)g_vinfo.xres;
  area.h = (fb_coord_t)g_vinfo.yres;

  ret = ioctl(g_fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));
  if (ret < 0)
    {
      printf("[DISP] FBIO_UPDATE failed: %d\n", errno);
    }

  return ret;
}

static void render_and_push(void)
{
  display_snapshot_t s;

  display_sample(&s);
  display_render(&s);
  display_push();

  g_prev = s;
}

static void *display_task(void *arg)
{
  int since_push = 0;

  (void)arg;

  /* 上电先立刻出一帧，不必等第一个周期 */

  render_and_push();

  for (;;)
    {
      display_snapshot_t s;

      sleep(DISPLAY_PERIOD_S);
      since_push += DISPLAY_PERIOD_S;

      display_sample(&s);

      /* 状态有变就重画；没变则每 DISPLAY_HEARTBEAT_S 秒兜底重推一帧 */

      if (display_changed(&s, &g_prev) || since_push >= DISPLAY_HEARTBEAT_S)
        {
          display_render(&s);
          display_push();
          g_prev = s;
          since_push = 0;
        }
    }

  return NULL;
}

int display_refresh_now(void)
{
  if (!g_ready)
    {
      return -ENODEV;
    }

  render_and_push();
  return 0;
}

bool display_is_ready(void)
{
  return g_ready;
}

int display_init(void)
{
  pthread_attr_t attr;
  int ret;

  if (g_ready)
    {
      return 0;
    }

  g_fd = open(DISPLAY_FBDEV, O_RDWR);
  if (g_fd < 0)
    {
      printf("[DISP] open %s failed: %d (display disabled)\n",
             DISPLAY_FBDEV, errno);
      return -errno;
    }

  if (ioctl(g_fd, FBIOGET_VIDEOINFO,
            (unsigned long)((uintptr_t)&g_vinfo)) < 0 ||
      ioctl(g_fd, FBIOGET_PLANEINFO,
            (unsigned long)((uintptr_t)&g_pinfo)) < 0)
    {
      printf("[DISP] FBIOGET_*INFO failed: %d\n", errno);
      close(g_fd);
      g_fd = -1;
      return -errno;
    }

  if (g_pinfo.bpp != 16)
    {
      printf("[DISP] unsupported bpp %d, need 16\n", g_pinfo.bpp);
      close(g_fd);
      g_fd = -1;
      return -ENOTSUP;
    }

  g_fb = mmap(NULL, g_pinfo.fblen, PROT_READ | PROT_WRITE,
              MAP_SHARED | MAP_FILE, g_fd, 0);
  if (g_fb == MAP_FAILED)
    {
      printf("[DISP] mmap failed: %d\n", errno);
      g_fb = NULL;
      close(g_fd);
      g_fd = -1;
      return -errno;
    }

  g_ready = true;

  printf("[DISP] %dx%d bpp=%d stride=%d fb=%p\n",
         g_vinfo.xres, g_vinfo.yres, g_pinfo.bpp, g_pinfo.stride, g_fb);

  /* 刷新线程：栈 4 KB 足够，本线程只用少量局部变量 */

  ret = pthread_attr_init(&attr);
  if (ret == 0)
    {
      (void)pthread_attr_setstacksize(&attr, 4096);
    }

  ret = pthread_create(&g_thread, &attr, display_task, NULL);
  (void)pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      printf("[DISP] pthread_create failed: %d\n", ret);
      g_ready = false;
      munmap(g_fb, g_pinfo.fblen);
      g_fb = NULL;
      close(g_fd);
      g_fd = -1;
      return -ret;
    }

  g_thread_started = true;
  return 0;
}

int display_deinit(void)
{
  if (!g_ready)
    {
      return -ENODEV;
    }

  if (g_thread_started)
    {
      pthread_cancel(g_thread);
      pthread_join(g_thread, NULL);
      g_thread_started = false;
    }

  g_ready = false;

  if (g_fb != NULL)
    {
      munmap(g_fb, g_pinfo.fblen);
      g_fb = NULL;
    }

  if (g_fd >= 0)
    {
      close(g_fd);
      g_fd = -1;
    }

  return 0;
}
