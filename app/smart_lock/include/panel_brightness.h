/****************************************************************************
 * app/smart_lock/include/panel_brightness.h
 *
 * 智锁卫士 - AMOLED 亮度调光
 *
 * 通过 CO5300 的 0x51（Write Display Brightness）寄存器调节板载 1.85"
 * AMOLED 的发光亮度。
 *
 * AMOLED 是自发光器件，没有背光电路、也没有 BL_PWM 引脚（黄山派的 22p
 * QSPI FPC 上确实没有 BL_PWM），所以"调光"只能是调面板自己的亮度寄存器，
 * 不能像 LCD 那样调背光 PWM。
 *
 * 实现细节与为什么必须绕路，见 src/panel_brightness.c 头部说明。
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __APP_SMART_LOCK_INCLUDE_PANEL_BRIGHTNESS_H
#define __APP_SMART_LOCK_INCLUDE_PANEL_BRIGHTNESS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 亮度取值范围（百分比）。0 表示最暗（面板仍点亮，但几乎不发光）。 */

#define PANEL_BRIGHTNESS_MIN      0
#define PANEL_BRIGHTNESS_MAX      100

/* 面板上电默认值：驱动 init 把 0x51 写成 0x7F，即 127/255 ≈ 50%。 */

#define PANEL_BRIGHTNESS_DEFAULT  50

/**
 * @brief 初始化亮度通路（幂等）
 *
 * 取 LCDC 句柄并读一次面板 ID 做自检。可以重复调用，只有第一次真正做事。
 *
 * @return 0 句柄已取得（是否通过自检另见 panel_brightness_verified()）,
 *         负 errno 表示句柄不可得
 */

int  panel_brightness_init(void);

/**
 * @brief 设置亮度
 *
 * 需要自检通过；未通过时返回 -EPERM，不会向面板写任何东西。
 *
 * @param percent 0-100，越界会被夹到范围内
 * @return 0 成功, 负 errno 失败
 */

int  panel_brightness_set(int percent);

/**
 * @brief 设置亮度（跳过自检，诊断用）
 *
 * 自检未通过但仍想验证硬件通路时使用。因为已经确认过句柄偏移与写入
 * 序列都是对的，"跳过自检"只影响信心，不影响安全性。
 *
 * @param percent 0-100
 * @return 0 成功, 负 errno 失败
 */

int  panel_brightness_force(int percent);

/**
 * @brief 最近一次成功写入的亮度百分比
 *
 * @return 0-100, -1 表示本次上电后从未成功设置过
 */

int  panel_brightness_get(void);

/**
 * @brief 现场回读面板的 0x52（Read Display Brightness）
 *
 * 本面板（CO5300）的 RDDISBV 是实现了的：读回来的就是 0x51 写进去的值，
 * 写 76 读回 76、写 0 读回 0，实测逐一对上。因此**可以**用它作为
 * "亮度寄存器确实被改成了目标值"的客观判据。
 *
 * 但它只反映寄存器内容，不代表肉眼看到的亮度已经变了 —— 视觉效果仍需看屏。
 * 见 .c 文件头说明。
 *
 * @return 0x00-0xFF 回读值, 负 errno 表示回读失败
 */

int  panel_brightness_readback(void);

/**
 * @brief 读面板的任意一个寄存器（诊断用，只读、不改硬件状态）
 *
 * @param reg 寄存器号
 * @param len 读几个字节, 1-4
 * @return 按小端拼装的读回值, 负 errno 表示失败
 */

int  panel_brightness_read_reg(uint8_t reg, int len);

/**
 * @brief 读面板 ID（0x04，3 字节）
 *
 * 本板应返回 0x331100。这是自检依据：能读到正确的 ID，说明 LCDC 句柄
 * 偏移正确、读通路可用。
 *
 * @return 面板 ID, 负 errno 表示失败
 */

int  panel_brightness_panel_id(void);

/**
 * @brief 百分比换算成 0x51 的原始值（与厂商 LCD_SetBrightness 同一映射）
 *
 * 供调用方显示"实际写下去的是什么"，避免把意图当成回读结果来汇报。
 *
 * @param percent 0-100
 * @return 0x00-0xFF
 */

int  panel_brightness_raw_for(int percent);

/**
 * @brief LCDC 句柄是否已取得
 */

bool panel_brightness_available(void);

/**
 * @brief 自检是否通过（读面板 ID 得到 0x331100），即写入前的准入条件
 */

bool panel_brightness_verified(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SMART_LOCK_INCLUDE_PANEL_BRIGHTNESS_H */
