CC = gcc
CFLAGS = -Wall -g -pthread

# 默认目标
all: main

# 根据参数选择不同的源文件组合
ifeq ($(filter test,$(MAKECMDGOALS)),test)
    SOURCES = test.c wfs.c
else
    SOURCES = main.c wfs.c
endif

# 生成目标文件名
OBJECTS = $(SOURCES:.c=.o)

# 目标可执行文件名
TARGET = main

# 链接步骤
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# 编译规则
%.o: %.c wfs.h
	$(CC) $(CFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -f $(TARGET) $(OBJECTS)

# 确保 make test 时会编译 test.c 和 wfs.c
.PHONY: test
test: clean all
