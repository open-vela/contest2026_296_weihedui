# tests/Makefile
#
# 测试用例构建文件
#
# Copyright (C) 2026 weihedui (Team 296)
# SPDX-License-Identifier: Apache-2.0

# 编译器
CC = gcc
CFLAGS = -Wall -Wextra -g -I../app/smart_lock/include -I.

# 测试目标
TEST_TARGETS = test_radar test_door_sensor

# 默认目标
all: $(TEST_TARGETS)

# 雷达测试
test_radar: test_radar.c
	$(CC) $(CFLAGS) -o $@ $<

# 门磁测试
test_door_sensor: test_door_sensor.c
	$(CC) $(CFLAGS) -o $@ $<

# 运行所有测试
run: $(TEST_TARGETS)
	@echo "Running tests..."
	@for test in $(TEST_TARGETS); do \
		echo ""; \
		echo "=== Running $$test ==="; \
		./$$test; \
	done

# 清理
clean:
	rm -f $(TEST_TARGETS)

.PHONY: all run clean
