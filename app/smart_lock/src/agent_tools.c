/**
 * @file agent_tools.c
 * @brief AI Agent工具函数实现 - 智锁卫士
 *
 * 实现ai_agent的Tool函数，包括：
 * - 雷达检查工具
 * - 门磁检查工具
 * - 电机控制工具
 * - 超时设置工具
 * - 系统状态工具
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <nuttx/config.h>

#include "smart_lock.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define JSON_BUFFER_SIZE 256

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * @brief 雷达检查工具
 *
 * 返回JSON格式的雷达状态，包括：
 * - presence: 是否有人
 * - zone: 区域 (indoor/outdoor)
 * - distance: 距离
 * - energy: 能量值
 * - state: 状态 (moving/stationary)
 */
int tool_radar_check(int argc, char **argv)
{
    radar_result_t result;
    int ret = radar_get_status(&result);

    if (ret < 0) {
        printf("{\"error\":\"radar read failed\"}\n");
        return -1;
    }

    printf("{\"presence\":%s,\"zone\":%s,\"distance\":%d,\"energy\":%d,\"state\":%s}\n",
           result.state != RADAR_STATE_NO_TARGET ? "true" : "false",
           result.zone == RADAR_ZONE_INDOOR ? "\"indoor\"" : "\"outdoor\"",
           result.distance,
           result.energy,
           result.state == RADAR_STATE_MOVING ? "\"moving\"" : "\"stationary\"");

    return 0;
}

/**
 * @brief 门磁检查工具
 *
 * 返回JSON格式的门状态，包括：
 * - closed: 是否关闭
 * - locked: 是否锁定
 * - door_state: 门状态 (closed/open)
 * - lock_state: 锁状态 (locked/unlocked)
 */
int tool_door_sensor_read(int argc, char **argv)
{
    door_status_t status;
    int ret = door_sensor_get_status(&status);

    if (ret < 0) {
        printf("{\"error\":\"door sensor read failed\"}\n");
        return -1;
    }

    printf("{\"closed\":%s,\"locked\":%s,\"door_state\":%s,\"lock_state\":%s}\n",
           status.door_state == DOOR_STATE_CLOSED ? "true" : "false",
           status.lock_state == LOCK_STATE_LOCKED ? "true" : "false",
           status.door_state == DOOR_STATE_CLOSED ? "\"closed\"" : "\"open\"",
           status.lock_state == LOCK_STATE_LOCKED ? "\"locked\"" : "\"unlocked\"");

    return 0;
}

/**
 * @brief 电机控制工具
 *
 * 接收控制命令并执行：
 * - open: 开门
 * - close: 关门
 * - lock: 锁门
 * - stop: 停止
 */
int tool_motor_control(int argc, char **argv)
{
    if (argc < 2) {
        printf("{\"error\":\"missing command\"}\n");
        return -1;
    }

    const char *cmd = argv[1];
    motor_cmd_t motor_cmd;

    if (strcmp(cmd, "open") == 0) {
        motor_cmd = MOTOR_CMD_OPEN;
    } else if (strcmp(cmd, "close") == 0) {
        motor_cmd = MOTOR_CMD_CLOSE;
    } else if (strcmp(cmd, "lock") == 0) {
        motor_cmd = MOTOR_CMD_LOCK;
    } else if (strcmp(cmd, "unlock") == 0) {
        motor_cmd = MOTOR_CMD_UNLOCK;
    } else if (strcmp(cmd, "stop") == 0) {
        motor_cmd = MOTOR_CMD_STOP;
    } else {
        printf("{\"error\":\"invalid command: %s\"}\n", cmd);
        return -1;
    }

    int ret = motor_control(motor_cmd);
    printf("{\"success\":%s,\"command\":\"%s\"}\n",
           ret == 0 ? "true" : "false", cmd);

    return ret;
}

/**
 * @brief 超时设置工具
 *
 * 设置自动关门时间（秒）
 */
int tool_set_timeout(int argc, char **argv)
{
    if (argc < 2) {
        printf("{\"error\":\"missing timeout value\"}\n");
        return -1;
    }

    int timeout = atoi(argv[1]);
    if (timeout <= 0 || timeout > 300) {
        printf("{\"error\":\"invalid timeout (1-300 seconds)\"}\n");
        return -1;
    }

    int ret = set_auto_close_timeout(timeout);
    printf("{\"success\":%s,\"timeout\":%d}\n",
           ret == 0 ? "true" : "false", timeout);

    return ret;
}

/**
 * @brief 系统状态工具
 *
 * 返回完整的系统状态，包括：
 * - door: 门状态
 * - lock: 锁状态
 * - radar: 雷达状态
 * - motor: 电机状态
 * - ble: BLE连接状态
 */
int tool_system_status(int argc, char **argv)
{
    door_status_t door_status;
    radar_result_t radar_status;
    motor_state_t motor_state;
    bool ble_connected;

    /* 获取各模块状态 */
    door_sensor_get_status(&door_status);
    radar_get_status(&radar_status);
    motor_state = motor_get_state();
    ble_connected = ble_is_connected();

    printf("{\n");
    printf("  \"door\": {\n");
    printf("    \"state\": \"%s\",\n",
           door_status.door_state == DOOR_STATE_CLOSED ? "closed" : "open");
    printf("    \"locked\": %s\n",
           door_status.lock_state == LOCK_STATE_LOCKED ? "true" : "false");
    printf("  },\n");
    printf("  \"radar\": {\n");
    printf("    \"presence\": %s,\n",
           radar_status.state != RADAR_STATE_NO_TARGET ? "true" : "false");
    printf("    \"distance\": %d,\n", radar_status.distance);
    printf("    \"state\": \"%s\"\n",
           radar_status.state == RADAR_STATE_MOVING ? "moving" : "stationary");
    printf("  },\n");
    printf("  \"motor\": {\n");
    printf("    \"state\": \"%s\",\n", motor_state_to_string(motor_state));
    printf("    \"running\": %s,\n",
           (motor_state == MOTOR_STATE_OPENING ||
            motor_state == MOTOR_STATE_CLOSING ||
            motor_state == MOTOR_STATE_LOCKING) ? "true" : "false");
    printf("    \"direction\": \"%s\"\n",
           motor_state == MOTOR_STATE_OPENING ? "opening" :
           motor_state == MOTOR_STATE_CLOSING ? "closing" :
           motor_state == MOTOR_STATE_LOCKING ? "locking" : "idle");
    printf("  },\n");
    printf("  \"ble\": {\n");
    printf("    \"connected\": %s\n", ble_connected ? "true" : "false");
    printf("  }\n");
    printf("}\n");

    return 0;
}

/**
 * @brief 屏幕亮度工具
 *
 * 调节板载 1.85" AMOLED 的发光亮度（CO5300 0x51 Write Display Brightness）。
 *
 * AMOLED 自发光，没有背光电路也没有 BL_PWM 引脚，所以"调光"只能是写面板
 * 自己的亮度寄存器 —— 实现见 src/panel_brightness.c。
 *
 * 用法：
 *   agent brightness              查询当前亮度与通路状态（不改动面板）
 *   agent brightness <0-100>      设置亮度
 *   agent brightness <0-100> force
 *                                 跳过自检强制写入（诊断用）
 *   agent brightness diag         只读地转储若干寄存器（诊断用）
 *
 * 默认（不带 force）要求启动自检通过：读面板 ID(0x04) 得到 0x331100。
 * 那一步同时验证了 LCDC 句柄偏移正确、读通路可用。自检不过就不写。
 *
 * 注意 readback 字段：本面板未实现 0x52，恒返回 0，**它不表示亮度**。
 * 这里如实输出但会同时给出 readback_note 说明，避免被误读成验证结果。
 */
int tool_brightness(int argc, char **argv)
{
    int ret;

    /* diag：只读转储，不写任何寄存器 */

    if (argc >= 2 && strcmp(argv[1], "diag") == 0) {
        static const struct { uint8_t reg; int len; const char *what; } probe[] = {
            { 0x04, 3, "panel_id"     },
            { 0x0A, 1, "power_mode"   },
            { 0x36, 1, "madctl"       },
            { 0x3A, 1, "colmod"       },
            { 0x51, 1, "wbright"      },
            { 0x52, 1, "rbright"      },
            { 0x53, 1, "wrctrld"      },
            { 0x63, 1, "wrhbmdl"      },
        };
        size_t i;

        (void)panel_brightness_init();
        printf("{");
        for (i = 0; i < sizeof(probe) / sizeof(probe[0]); i++) {
            int v = panel_brightness_read_reg(probe[i].reg, probe[i].len);
            printf("%s\"%s\":", i ? "," : "", probe[i].what);
            if (v >= 0) {
                printf("\"0x%0*X\"", probe[i].len * 2, (unsigned)v);
            } else {
                printf("null");
            }
        }
        printf("}\n");
        return 0;
    }

    /* 无参数：只报告，不写面板 */

    if (argc < 2) {
        char pctbuf[16];
        char idbuf[16];
        char rawbuf[16];
        int  pct;
        int  id;
        int  raw;

        ret = panel_brightness_init();
        pct = panel_brightness_get();
        id  = panel_brightness_panel_id();
        raw = panel_brightness_readback();

        if (pct >= 0) {
            snprintf(pctbuf, sizeof(pctbuf), "%d", pct);
        } else {
            snprintf(pctbuf, sizeof(pctbuf), "null");
        }
        if (id >= 0) {
            /* 注意带引号：panel_id 是十六进制字符串，不加引号会输出
             * "panel_id":0x331100 这种非法 JSON。
             */
            snprintf(idbuf, sizeof(idbuf), "\"0x%06X\"", (unsigned)id);
        } else {
            snprintf(idbuf, sizeof(idbuf), "null");
        }
        if (raw >= 0) {
            snprintf(rawbuf, sizeof(rawbuf), "%d", raw);
        } else {
            snprintf(rawbuf, sizeof(rawbuf), "null");
        }

        printf("{\"available\":%s,\"verified\":%s,\"panel_id\":%s,"
               "\"percent\":%s,\"readback\":%s,"
               "\"readback_note\":\"0x52 mirrors 0x51; equals raw_written after a set\"}\n",
               panel_brightness_available() ? "true" : "false",
               panel_brightness_verified() ? "true" : "false",
               idbuf, pctbuf, rawbuf);

        return ret < 0 ? ret : 0;
    }

    /* 有参数：设置亮度 */

    {
        int percent = atoi(argv[1]);
        bool force = (argc >= 3 && strcmp(argv[2], "force") == 0);

        if (percent < PANEL_BRIGHTNESS_MIN || percent > PANEL_BRIGHTNESS_MAX) {
            printf("{\"error\":\"invalid brightness (0-100)\"}\n");
            return -1;
        }

        ret = force ? panel_brightness_force(percent)
                    : panel_brightness_set(percent);

        if (ret < 0) {
            printf("{\"success\":false,\"percent\":%d,\"error\":\"%s\","
                   "\"verified\":%s}\n",
                   percent,
                   ret == -EPERM ? "self-test not passed, retry with 'force'"
                                 : "panel write failed",
                   panel_brightness_verified() ? "true" : "false");
            return ret;
        }

        /* raw_written 是写进 0x51 的意图值。
         *
         * 回读 0x52 本是理想验证手段，但实测发现：DDIC 的寄存器更新有微小
         * 延迟（约一帧），写完立刻读拿到的是旧值、不是刚写进去的值。这在
         * 快速连写时尤其明显：brightness 30 → 立即回读 = 127（上电默认值）；
         * 但几秒后再查 agent brightness → readback = 76，完全正确。
         *
         * 因此**不在 set 响应里做即时回读**（它只会引入困惑），改为把回读
         * 留给显式的查询路径（无参数 agent brightness），那时延迟已过、
         * 回读是准的。
         */

        printf("{\"success\":true,\"percent\":%d,\"raw_written\":%d,"
               "\"forced\":%s}\n",
               percent, panel_brightness_raw_for(percent),
               force ? "true" : "false");

        return 0;
    }
}
