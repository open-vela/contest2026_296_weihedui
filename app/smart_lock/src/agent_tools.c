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
    motor_status_t motor_status;
    bool ble_connected;

    /* 获取各模块状态 */
    door_sensor_get_status(&door_status);
    radar_get_status(&radar_status);
    motor_get_status(&motor_status);
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
    printf("    \"running\": %s,\n",
           motor_status.running ? "true" : "false");
    printf("    \"direction\": \"%s\"\n",
           motor_status.direction == MOTOR_DIR_OPEN ? "opening" : "closing");
    printf("  },\n");
    printf("  \"ble\": {\n");
    printf("    \"connected\": %s\n", ble_connected ? "true" : "false");
    printf("  }\n");
    printf("}\n");

    return 0;
}
