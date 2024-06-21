# 定义项目名称
PROJECT_NAME := server

# 定义编译器
CC := gcc

# 定义编译选项
CFLAGS := -Wall -g -I. # -I. 表示包含当前目录的头文件

# 定义链接选项
LDFLAGS := -pthread # 如果使用了线程库，需要链接 pthread

# 定义源文件列表
SRCS := main.c thread.c web.c log.c

# 定义目标文件列表
OBJS := $(SRCS:.c=.o)

# 默认目标
all: $(PROJECT_NAME)

# 链接目标文件生成最终的可执行文件
$(PROJECT_NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 编译单个源文件生成目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理编译生成的文件
clean:
	rm -f $(OBJS) $(PROJECT_NAME)

# 伪目标，确保 make 命令总是执行
.PHONY: all clean