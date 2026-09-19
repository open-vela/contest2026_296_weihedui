/****************************************************************************
 * app/smart_lock/include/display.h
 *
 * 智锁卫士 - AMOLED 状态显示模块
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __APP_SMART_LOCK_INCLUDE_DISPLAY_H
#define __APP_SMART_LOCK_INCLUDE_DISPLAY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化显示模块并启动刷新线程
 *
 * 打开 /dev/fb0、mmap 帧缓冲，然后创建一个 1 Hz 的后台线程把门锁状态
 * 渲染到 AMOLED 上。
 *
 * 只有整屏更新是可靠的（见 src/lcdtest.c 的说明），因此本模块每帧都往
 * 帧缓冲里重画整幅画面，再用**一次覆盖全屏的** FBIO_UPDATE 推上屏。
 *
 * 若 /dev/fb0 打不开（例如无屏配置），返回负值，调用方忽略即可 ——
 * 显示对门锁功能是纯附加的，不参与任何安全逻辑。
 *
 * @return 0 成功, 负值失败
 */

int display_init(void);

/**
 * @brief 停止刷新线程并释放帧缓冲映射
 *
 * @return 0 成功, 负值失败
 */

int display_deinit(void);

/**
 * @brief 立即渲染并推屏一帧（同步，调试用）
 *
 * @return 0 成功, 负值失败
 */

int display_refresh_now(void);

/**
 * @brief 显示模块是否已就绪
 *
 * @return true 已就绪, false 未初始化或初始化失败
 */

bool display_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SMART_LOCK_INCLUDE_DISPLAY_H */
