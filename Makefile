# Makefile for smart_lock modules

CC = gcc
CFLAGS = -Wall -Wextra -I app/smart_lock/include -lpthread
LDFLAGS = -lpthread

# Source files
MOTOR_SRC = app/smart_lock/src/door_control.c
SAFETY_SRC = app/smart_lock/src/safety.c

# Test files
TEST_MOTOR_SRC = tests/test_motor.c
TEST_SAFETY_SRC = tests/test_safety.c

# Output binaries
TEST_MOTOR_BIN = tests/test_motor
TEST_SAFETY_BIN = tests/test_safety

.PHONY: all clean test test-motor test-safety

all: test-motor test-safety

test-motor: $(TEST_MOTOR_BIN)
	@echo "Running motor tests..."
	@./$(TEST_MOTOR_BIN)

test-safety: $(TEST_SAFETY_BIN)
	@echo "Running safety tests..."
	@./$(TEST_SAFETY_BIN)

$(TEST_MOTOR_BIN): $(TEST_MOTOR_SRC) $(MOTOR_SRC)
	$(CC) -o $@ $^ $(CFLAGS)

$(TEST_SAFETY_BIN): $(TEST_SAFETY_SRC) $(SAFETY_SRC) $(MOTOR_SRC)
	$(CC) -o $@ $^ $(CFLAGS)

test: test-motor test-safety

clean:
	rm -f $(TEST_MOTOR_BIN) $(TEST_SAFETY_BIN)
