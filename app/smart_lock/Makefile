############################################################################
# app/smart_lock/Makefile
#
# 智锁卫士 - Team 296 (weihedui)
#
# 此前该文件缺失。参照 packages/demos/contest2026_296_hello_app/Makefile。
#
# Copyright (C) 2026 weihedui (Team 296)
# SPDX-License-Identifier: Apache-2.0
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = smart_lock
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 8192
MODULE    = $(CONFIG_SMART_LOCK)

CSRCS = src/radar_driver.c \
        src/door_sensor.c \
        src/door_control.c \
        src/safety.c \
        src/ble_service.c \
        src/agent_tools.c

MAINSRC = src/smart_lock_main.c

# 头文件位于本应用的 include/ 子目录

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_296_smart_lock/include

include $(APPDIR)/Application.mk
