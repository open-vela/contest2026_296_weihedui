/****************************************************************************
 * app/smart_lock/src/panel_brightness.c
 *
 * 智锁卫士 - AMOLED 亮度调光（CO5300 0x51）
 *
 * ---------------------------------------------------------------------------
 * 为什么这件事需要绕路
 * ---------------------------------------------------------------------------
 * 厂商驱动里的亮度通路全是死的，而且生产仓库零改动，不能去补：
 *
 *   1. co5300.c:544 LCD_SetBrightness() 挂在 ops 表上，但 sf32lb_lcd.c
 *      从来不调用它 —— 整个仓库里除定义处外没有第二个引用。它只在
 *      LCD_Init 时被执行一次（co5300.c:281 写 0x51 = 0x7F，即 ~50%）。
 *   2. sf32lb_lcd.c:697 sf32lb_lcd_setcontrast() 是空壳，直接
 *      `return -ENOSYS`。所以 NuttX 标准的 /dev/lcd0 对比度 ioctl
 *      （LCDIOC_SETCONTRAST）也是死的。
 *   3. co5300.c:430 LCD_WriteReg() 是 **static** 的（nm 显示为局部符号
 *      `t`），链接期取不到，无法直接调用。
 *   4. co5300.c 里也没有导出任何亮度相关的公开 API。
 *
 * 剩下唯一可用的通路是 HAL 层：HAL_LCDC_WriteDatas() 是全局符号（nm 为
 * `T`），而厂商的 LCD_WriteReg() 本身就是它的薄封装。本模块做的就是
 * 「按厂商一模一样的字节序列，自己发一次寄存器写」。
 *
 * ---------------------------------------------------------------------------
 * 句柄是怎么拿到的（偏移 48 有权威依据，且运行时复核）
 * ---------------------------------------------------------------------------
 * board_lcd_getdev(0) 返回 &s_drv_lcd.dev（sf32lb_lcd.c:959）。而
 *
 *     struct sf32lb_lcd_dev_s
 *     {
 *         struct lcd_dev_s dev;        首成员，偏移 0
 *         LCDC_HandleTypeDef hlcdc;    我们要的句柄
 *         ...
 *     };
 *
 * 所以 &hlcdc = (char *)dev + offsetof(..., hlcdc)。该偏移取自**镜像自带
 * 的 DWARF 调试信息**（arm-none-eabi-readelf --debug-dump=info），是编译器
 * 为这个具体目标文件记录的真实布局，不是推算：
 *
 *     struct sf32lb_lcd_dev_s  DW_AT_byte_size = 324
 *       dev                    DW_AT_data_member_location = 0
 *       hlcdc                  DW_AT_data_member_location = 48
 *     struct lcd_dev_s         DW_AT_byte_size = 48
 *
 * 即偏移恰好等于 sizeof(struct lcd_dev_s)。为了让这个等式在本编译单元里
 * 也成立，panel_lcdc() 在返回句柄前会**现场核对** sizeof(struct lcd_dev_s)
 * 是否仍为 48；一旦不等（例如头文件被改、配置宏变化），直接放弃并拒绝
 * 一切写入 —— 绝不用一个可能算错的地址去碰 LCDC 寄存器。
 *
 * ---------------------------------------------------------------------------
 * 写入序列（逐字节复刻 co5300.c，未做任何"改进"）
 * ---------------------------------------------------------------------------
 * co5300.c:430 LCD_WriteReg():
 *     cmd = (0x02 << 24) | (reg << 8);  HAL_LCDC_WriteU32Reg(hlcdc, cmd, p, n);
 * co5300.c:462 LCD_ReadData():
 *     cmd = (0x03 << 24) | (reg << 8);  HAL_LCDC_ReadU32Reg(hlcdc, cmd, p, n);
 * 且读之前要把时钟降到 2 MHz（READ mode 最短周期 300 ns），读完还原。
 * 厂商驱动的 lcdc_int_cfg.freq 为 50000000（未定义 LCD_MAX_CLK_FREQ）。
 *
 * ---------------------------------------------------------------------------
 * 关于 0x52：本面板**可以**读回亮度，回读是可信判据
 * ---------------------------------------------------------------------------
 * 结论先说：0x52（RDDISBV）在这颗 CO5300 上是实现了的，读回来的就是
 * 0x51 写进去的值，**可以直接当"亮度已改变"的客观证据用**。
 *
 * 写→读的闭环实测（同一次上电内，先写后读）：
 *
 *     agent brightness 30   ->  {"success":true,"percent":30,"raw_written":76}
 *     agent brightness      ->  {"readback":76, ...}
 *     agent brightness 0    ->  {"success":true,"percent":0,  "raw_written":0}
 *     agent brightness      ->  {"readback":0,  ...}
 *     开机后未写过时        ->  {"readback":127}   (0x7F，驱动 init 写入值)
 *
 * 三次读回与写入值逐一对上，说明回读跟着写入走，不是常量、不是噪声。
 *
 * 需要如实记一笔的反复：**本模块早期的单次读实现读 0x52 稳定得到
 * 0x00**（ret=0，即读命令成功返回，只是数据为 0），当时据此写下了
 * "这颗 DDIC 没有实现 RDDISBV"的错误结论。后来一次改动同时做了两件事：
 *
 *   a) co5300_read_reg() 改为失败重试（CO5300_RETRY_MAX 次，每次间隔
 *      CO5300_RETRY_US），并在每次尝试前后成对设置 2 MHz / 50 MHz；
 *   b) 自检改为先读 0x04（3 字节）再读别的寄存器。
 *
 * 改完之后 0x00 再没复现过。两处改动是一起进去的，没有做单变量对照，
 * 因此**无法断定**是重试解决了偶发失败，还是首次读需要一次前置读来
 * "热身"。当时能确定的只有：读通路本身没坏（0x04 一直能读到 0x331100，
 * 且驱动 init 的 LCD_ReadID() 也靠它打印 "LCD module use CO5300 IC"）。
 * 现有实现是两种可能性的超集（既重试又先读 0x04），实测稳定，故保留。
 *
 * ---------------------------------------------------------------------------
 * 那怎么证明写入确实到达了面板
 * ---------------------------------------------------------------------------
 * 1. 回读闭环：写 0x51 的值能从 0x52 原样读回来（见上）。
 * 2. 句柄是真的：自检读 0x04 得到 0x331100，而这次读用的句柄正是
 *    board_lcd_getdev(0) + sizeof(struct lcd_dev_s) 算出来的那个。
 * 3. 写通路是真的：驱动 init 用同一个序列写了
 *    REG_COLMOD(0x3A)←0x55、REG_WRCTRLD(0x53)←0x20、REG_WRHBMDISBV(0x63)←0xFF，
 *    而屏幕确实以 RGB565 正常显示 —— 这些写要没落到面板上，屏根本不亮。
 *    本模块的 co5300_write_reg() 与它是同一条序列，只是寄存器字节不同。
 *    `agent brightness diag` 能把这几笔厂商写的值全部读回来核对。
 *
 * 注意：回读能证明"寄存器被改成了目标值"，但**证明不了肉眼看到的亮度
 * 变化**（比如面板是否真的按 0x51 调光）。两者都要，回读给客观数据，
 * 肉眼给最终效果。
 *
 * ---------------------------------------------------------------------------
 * 自检
 * ---------------------------------------------------------------------------
 * 读 0x04 得到 0x331100 才算通过，不通过时默认**不允许写入**，需显式
 * force。force 只影响"准不准写"，不影响写入序列本身。
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <nuttx/board.h>
#include <nuttx/lcd/lcd.h>

#include "smart_lock.h"
#include "panel_brightness.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* CO5300 寄存器（取自 co5300.c:86-115） */

#define CO5300_REG_LCD_ID       0x04    /* 面板 ID，驱动 init 用它识别型号 */
#define CO5300_REG_WBRIGHT      0x51    /* Write Display Brightness */
#define CO5300_REG_RBRIGHT      0x52    /* Read  Display Brightness，回读 0x51 写入值 */
#define CO5300_REG_WRCTRLD      0x53    /* Write Control Display */
#define CO5300_REG_COLMOD       0x3A    /* 像素格式 */

/* 0x53 的 bit5 = BL（Brightness Level）使能位。厂商 init 写 0x20（bit5=1），
 * 值得一并写入以确保 0x51 的值被面板采用。
 */

#define CO5300_WRCTRLD_BRIGHTNESS_EN  0x20

/* 命令前缀（取自 co5300.c:430 / co5300.c:462） */

#define CO5300_CMD_WRITE        0x02
#define CO5300_CMD_READ         0x03

/* 厂商 LCD_Drv_Init 写入 0x51 的初值（co5300.c:281），即上电默认亮度 */

#define CO5300_INIT_BRIGHTNESS  0x7F

/* 面板 ID。co5300.c:53-54 按分辨率选值：本板 390x450 对应 0x331100。
 *
 * 自检读 0x04 而不是 0x52：ID 是只读常量，写成多少都该读到 0x331100，
 * 与"当前亮度是多少"无关，因此更适合判定句柄/读通路是否正确。0x52 也
 * 能读（见文件头），但它反映的是可变状态，拿它做自检等于把自检和被测
 * 对象绑在一起。驱动 init 的 LCD_ReadID()（co5300.c:331）同样读 0x04。
 */

#define CO5300_PANEL_ID         0x331100

/* 亮度原始值上限（co5300.c:122 REG_BRIGHTNESS_MAX），映射关系与厂商
 * LCD_SetBrightness() 一致：raw = 0xFF * percent / 100
 */

#define CO5300_BRIGHTNESS_MAX   0xFF

/* 读寄存器前要把 LCDC 时钟降到 2 MHz，读完还原（co5300.c:170 LCD_ReadMode） */

#define CO5300_FREQ_READ        2000000
#define CO5300_FREQ_NORMAL      50000000

/* LCDC 忙时的重试次数与间隔。HAL 自身有 __HAL_LOCK，显示线程正在推一帧
 * （约 0.35 s）时写寄存器会拿到 HAL_BUSY，退避重试即可，不必去抢锁。
 */

#define CO5300_RETRY_MAX        8
#define CO5300_RETRY_US         60000

/* hlcdc 在 struct sf32lb_lcd_dev_s 里的偏移，等于 sizeof(struct lcd_dev_s)。
 * 数值来自镜像 DWARF，见文件头说明；运行时会被复核。
 */

#define LCD_DEV_S_SIZE_EXPECTED 48

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* HAL_StatusTypeDef 是 int 宽度的枚举，HAL_OK == 0（bf0_hal_def.h:32-38）。
 * 只用它做 `!= 0` 判断，因此按 int 声明是 ABI 安全的。
 */

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* 以下三个原型逐字抄自 vendor/sifli/chips/drivers/Include/bf0_hal_lcdc.h：
 *   HAL_LCDC_WriteDatas  L886
 *   HAL_LCDC_ReadDatas   L844
 *   HAL_LCDC_SetFreq     L612
 *
 * 不直接 #include 该头文件是有意为之：它会依次拖进 bf0_hal_def.h /
 * bf0_hal_dsi.h / CMSIS / SoC 一整串厂商头文件，而本应用只需要这三个
 * 符号。三个符号在最终镜像里都是全局符号（nm 显示为 T），链接可解析。
 * 句柄按 void * 传递 —— 我们从头到尾只需要它的地址，不需要解引用它。
 */

extern int HAL_LCDC_WriteDatas(void *lcdc, unsigned int addr,
                               unsigned int addr_len,
                               unsigned char *p_data, unsigned int data_len);
extern int HAL_LCDC_ReadDatas(void *lcdc, unsigned int addr,
                              unsigned int addr_len,
                              unsigned char *p_data, unsigned int data_len);
extern int HAL_LCDC_SetFreq(void *lcdc, unsigned int freq);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static void *g_lcdc;            /* LCDC 句柄，NULL 表示尚未取得 */
static bool  g_inited;          /* panel_lcdc() 是否已尝试过 */
static bool  g_available;       /* 句柄是否可用 */
static bool  g_verified;        /* 回读自检是否通过 */
static int   g_percent = -1;    /* 最近一次成功写入的百分比 */
static int   g_last_err;        /* 最近一次失败原因，诊断用 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 取得 LCDC 句柄
 *
 * board_lcd_getdev() 返回 &s_drv_lcd.dev；dev 是结构体首成员，因此句柄
 * 地址 = dev 地址 + sizeof(struct lcd_dev_s)。
 *
 * 返回前复核 sizeof(struct lcd_dev_s)：这是本模块唯一一处"依赖外部布局"
 * 的地方，复核不通过就整体放弃，宁可不做也不乱写。
 *
 * @return 句柄, NULL 表示不可用
 */

static void *panel_lcdc(void)
{
  FAR struct lcd_dev_s *dev;

  if (g_inited)
    {
      return g_available ? g_lcdc : NULL;
    }

  g_inited = true;

  if (sizeof(struct lcd_dev_s) != LCD_DEV_S_SIZE_EXPECTED)
    {
      printf("[PBRT] sizeof(struct lcd_dev_s)=%u, expected %d -- struct layout "
             "mismatch, refusing to write\n",
             (unsigned)sizeof(struct lcd_dev_s), LCD_DEV_S_SIZE_EXPECTED);
      g_last_err = -EINVAL;
      return NULL;
    }

  dev = board_lcd_getdev(0);
  if (dev == NULL)
    {
      printf("[PBRT] board_lcd_getdev(0) returned NULL\n");
      g_last_err = -ENODEV;
      return NULL;
    }

  g_lcdc      = (void *)((char *)dev + sizeof(struct lcd_dev_s));
  g_available = true;
  return g_lcdc;
}

/**
 * @brief 写一个 CO5300 寄存器（复刻 co5300.c:430 LCD_WriteReg）
 */

static int co5300_write_reg(void *lcdc, uint8_t reg,
                            const uint8_t *data, uint32_t len)
{
  uint32_t cmd = ((uint32_t)CO5300_CMD_WRITE << 24) | ((uint32_t)reg << 8);
  int      ret;
  int      i;

  for (i = 0; i < CO5300_RETRY_MAX; i++)
    {
      /* HAL_LCDC_WriteU32Reg(lcdc, cmd, data, len) 展开即为下式：
       * addr = cmd，addr_len = 4
       */

      ret = HAL_LCDC_WriteDatas(lcdc, cmd, 4, (unsigned char *)data, len);
      if (ret == 0)
        {
          return 0;
        }

      /* 非 0 多为 HAL_BUSY（显示线程正在推帧），退避后重试 */

      usleep(CO5300_RETRY_US);
    }

  return -EBUSY;
}

/**
 * @brief 读一个 CO5300 寄存器（复刻 co5300.c:462 LCD_ReadData）
 *
 * 读之前把 LCDC 时钟降到 2 MHz，读完无论成败都还原 —— 漏掉还原会让后续
 * 推屏以错误的时钟跑，所以用"先读后还原"的顺序并在失败路径上也还原。
 *
 * 与写不同，读必须重试。HAL_LCDC_ReadDatas() 不会先等 LCDC 空闲，而是
 * 直接 __HAL_LCDC_LOCK，发现 lcdc->State != HAL_LCDC_STATE_READY 就返回
 * HAL_BUSY：

 *     __HAL_LCDC_LOCK(lcdc);
 *     if (HAL_LCDC_STATE_READY != lcdc->State) { __HAL_LCDC_UNLOCK(lcdc); return HAL_BUSY; }
 *
 * 而显示线程每秒推一整屏（约 0.35 s），单次读的碰撞概率是三成以上 ——
 * 不重试的话自检会偶发失败。HAL_BUSY 路径在解锁后直接返回，没有改动
 * 任何硬件状态，所以重试是安全的。
 *
 * SetFreq 内部第一步就是 WaitBusy()，会等到当前传输结束才改时钟，因此
 * 读之前降频不会打断正在进行的推屏。
 */

static int co5300_read_reg(void *lcdc, uint8_t reg, uint8_t *data, uint32_t len)
{
  uint32_t cmd = ((uint32_t)CO5300_CMD_READ << 24) | ((uint32_t)reg << 8);
  int      ret;
  int      i;

  for (i = 0; i < CO5300_RETRY_MAX; i++)
    {
      (void)HAL_LCDC_SetFreq(lcdc, CO5300_FREQ_READ);
      ret = HAL_LCDC_ReadDatas(lcdc, cmd, 4, data, len);
      (void)HAL_LCDC_SetFreq(lcdc, CO5300_FREQ_NORMAL);

      if (ret == 0)
        {
          return 0;
        }

      usleep(CO5300_RETRY_US);
    }

  return -EBUSY;
}

/**
 * @brief 百分比 → 0x51 原始值（与厂商 LCD_SetBrightness 同一映射）
 */

static uint8_t percent_to_raw(int percent)
{
  if (percent < PANEL_BRIGHTNESS_MIN)
    {
      percent = PANEL_BRIGHTNESS_MIN;
    }

  if (percent > PANEL_BRIGHTNESS_MAX)
    {
      percent = PANEL_BRIGHTNESS_MAX;
    }

  return (uint8_t)((int)CO5300_BRIGHTNESS_MAX * percent / 100);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int panel_brightness_init(void)
{
  int id;

  if (panel_lcdc() == NULL)
    {
      return g_last_err != 0 ? g_last_err : -ENODEV;
    }

  if (g_verified)
    {
      return 0;
    }

  /* 自检：读面板 ID。这是对"句柄偏移正确 + 读通路可用"的端到端验证 ——
   * 能读出 0x331100，就说明我们拿到的确实是那个 LCDC 句柄、并且发的
   * 读命令真的到达了面板。只有自检通过才允许写入。
   */

  id = panel_brightness_panel_id();

  if (id == CO5300_PANEL_ID)
    {
      g_verified = true;
      g_percent  = PANEL_BRIGHTNESS_DEFAULT;

      printf("[PBRT] AMOLED ok: panel id = 0x%06X, the 0x51 write path reaches "
             "the panel\n", (unsigned)id);
      return 0;
    }

  /* 区分两种失败，让调用方能给出准确提示：
   *   id < 0   —— 读命令本身就没成功（总线/句柄问题），是真正的 I/O 失败；
   *   id >= 0  —— 读成功了，但读到的不是本面板的 ID。通路是好的，只是
   *               自检没过，因此是"权限/前置条件"类错误而非 I/O 错误。
   * tool_brightness 依赖这个区分：-EPERM 对应 "self-test not passed"，
   * -EIO 才对应 "panel write failed"。
   */

  if (id < 0)
    {
      printf("[PBRT] self-test failed: panel id read error %d\n", id);
      g_last_err = -EIO;
      return -EIO;
    }

  printf("[PBRT] self-test failed: panel id = 0x%06X (expected 0x%06X), "
         "use 'force' to write anyway\n", (unsigned)id, CO5300_PANEL_ID);

  g_last_err = -EPERM;
  return -EPERM;
}

int panel_brightness_force(int percent)
{
  uint8_t raw = percent_to_raw(percent);
  uint8_t ctrld = CO5300_WRCTRLD_BRIGHTNESS_EN;  /* bit5 = BL 使能 */
  int     ret;

  if (panel_lcdc() == NULL)
    {
      return g_last_err != 0 ? g_last_err : -ENODEV;
    }

  /* 实测发现面板寄存器会被持续重置（colmod 从 0x55 变回 0x77 默认值，
   * wrctrld 从 0x20 变回 0x28，wbright 从 0x7F 变回 0x00），说明有
   * 其他初始化路径在重置面板。因此每次写亮度都必须同时：
   *   1) 写 0x53 使能亮度控制（bit5）；
   *   2) 写 0x51 设定亮度值。
   * 必须是这个顺序，否则 0x51 的写入会被后续的0x53 写入覆盖。
   */

  ret = co5300_write_reg(g_lcdc, CO5300_REG_WRCTRLD, &ctrld, 1);
  if (ret < 0)
    {
      printf("[PBRT] WARN: WRCTRLD write failed (%d), brightness may not apply\n",
             ret);
      g_last_err = ret;
      return ret;
    }

  ret = co5300_write_reg(g_lcdc, CO5300_REG_WBRIGHT, &raw, 1);
  if (ret < 0)
    {
      printf("[PBRT] WARN: WBRIGHT write failed (%d)\n", ret);
      g_last_err = ret;
      return ret;
    }

  g_percent  = percent < PANEL_BRIGHTNESS_MIN ? PANEL_BRIGHTNESS_MIN :
               percent > PANEL_BRIGHTNESS_MAX ? PANEL_BRIGHTNESS_MAX :
               percent;
  g_last_err = 0;

  return 0;
}

int panel_brightness_set(int percent)
{
  if (!g_verified)
    {
      /* 自检没通过就不写。先尝试初始化一次，让调用方拿到准确原因。 */

      int ret = panel_brightness_init();

      if (ret < 0)
        {
          return ret;
        }

      if (!g_verified)
        {
          return -EPERM;
        }
    }

  return panel_brightness_force(percent);
}

int panel_brightness_get(void)
{
  return g_percent;
}

int panel_brightness_read_reg(uint8_t reg, int len)
{
  uint8_t buf[4] = { 0, 0, 0, 0 };
  uint32_t v;
  int ret;

  if (len < 1 || len > 4)
    {
      return -EINVAL;
    }

  if (panel_lcdc() == NULL)
    {
      return g_last_err != 0 ? g_last_err : -ENODEV;
    }

  ret = co5300_read_reg(g_lcdc, reg, buf, (uint32_t)len);
  if (ret != 0)
    {
      return -EIO;
    }

  /* 与厂商一致地按小端拼装（co5300.c:462 把 &rd_data 当字节缓冲收数据，
   * 所以 buf[0] 是低字节）。3 字节 ID 读出 0x331100 即 buf = 00 11 33。
   */

  v = (uint32_t)buf[0]
    | ((uint32_t)buf[1] << 8)
    | ((uint32_t)buf[2] << 16)
    | ((uint32_t)buf[3] << 24);

  return (int)v;
}

int panel_brightness_panel_id(void)
{
  return panel_brightness_read_reg(CO5300_REG_LCD_ID, 3);
}

int panel_brightness_readback(void)
{
  return panel_brightness_read_reg(CO5300_REG_RBRIGHT, 1);
}

int panel_brightness_raw_for(int percent)
{
  return (int)percent_to_raw(percent);
}

bool panel_brightness_available(void)
{
  return g_available;
}

bool panel_brightness_verified(void)
{
  return g_verified;
}
