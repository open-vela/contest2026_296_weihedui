/****************************************************************************
 * app/smart_lock/src/agent_main.c
 *
 * 智锁卫士 - AI Agent 工具命令入口
 *
 * docs/communication/agent_guide.md 约定了如下 NSH 命令界面：
 *
 *     agent radar_check
 *     agent door_sensor_read
 *     agent motor_control open|close|lock|unlock|stop
 *     agent set_timeout <1-300>
 *     agent system_status
 *     agent brightness [0-100] [force]
 *
 * 但原提交只写了 src/agent_tools.c 里的五个 tool_*() 实现，全仓库没有任何
 * 地方注册命令、也没有任何调用者，因此这五个函数是死代码（即使编进镜像，
 * 链接期也会被 --gc-sections 全部丢弃）。本文件补上命令解析与分发，
 * 并在 CMakeLists.txt 中通过 nuttx_add_application(NAME agent ...) 把它
 * 注册成 NSH 内置命令。
 *
 * Copyright (C) 2026 weihedui (Team 296)
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <nuttx/config.h>

#include "smart_lock.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 打印命令用法
 */

static void agent_usage(void)
{
    printf("Usage: agent <tool> [args]\n");
    printf("  agent radar_check                                雷达检测状态\n");
    printf("  agent door_sensor_read                           门磁状态\n");
    printf("  agent motor_control open|close|lock|unlock|stop  电机控制\n");
    printf("  agent set_timeout <seconds>                      设置自动关门超时(1-300)\n");
    printf("  agent system_status                              完整系统状态\n");
    printf("  agent brightness [0-100] [force]                 屏幕亮度(无参数=查询)\n");
    printf("  agent brightness diag                            只读转储面板寄存器\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * @brief agent 命令入口
 *
 * 由 nuttx_add_application(NAME agent ...) 注册为 NSH 内置命令，生成的内置
 * 表项为 { "agent", <priority>, <stacksize>, agent_main }。
 *
 * 分发时把参数前移一位（argc - 1, argv + 1），使各 tool_*() 看到的 argv[1]
 * 正好是它们期望的"第一个参数"：
 *
 *   NSH  收到: argv[0]="agent" argv[1]="motor_control" argv[2]="open"
 *   tool 收到: argv[0]="motor_control" argv[1]="open"
 *
 * @param argc 参数个数
 * @param argv 参数列表
 * @return int 0: 成功, -EINVAL: 参数非法或工具名未知
 */

int agent_main(int argc, char *argv[])
{
    const char *tool;

    if (argc < 2)
    {
        printf("{\"error\":\"missing tool name\"}\n");
        agent_usage();
        return -EINVAL;
    }

    tool = argv[1];

    if (strcmp(tool, "radar_check") == 0)
    {
        return tool_radar_check(argc - 1, argv + 1);
    }

    if (strcmp(tool, "door_sensor_read") == 0)
    {
        return tool_door_sensor_read(argc - 1, argv + 1);
    }

    if (strcmp(tool, "motor_control") == 0)
    {
        return tool_motor_control(argc - 1, argv + 1);
    }

    if (strcmp(tool, "set_timeout") == 0)
    {
        return tool_set_timeout(argc - 1, argv + 1);
    }

    if (strcmp(tool, "system_status") == 0)
    {
        return tool_system_status(argc - 1, argv + 1);
    }

    if (strcmp(tool, "brightness") == 0)
    {
        return tool_brightness(argc - 1, argv + 1);
    }

    if (strcmp(tool, "help") == 0 || strcmp(tool, "-h") == 0)
    {
        agent_usage();
        return 0;
    }

    printf("{\"error\":\"unknown tool: %s\"}\n", tool);
    agent_usage();

    return -EINVAL;
}
