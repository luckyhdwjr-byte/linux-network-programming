# 定义编译器和选项
CC = gcc
CXX = g++
CFLAGS = -Wall -g
CXXFLAGS = -Wall -g -std=c++11
LDFLAGS = -lpthread -levent

# 自动获取所有的 .c 和 .cpp 文件
C_SRCS = $(wildcard *.c)
CPP_SRCS = $(wildcard *.cpp)

# 将源文件名转换为不带后缀的目标程序名
C_BINS = $(C_SRCS:.c=)
CPP_BINS = $(CPP_SRCS:.cpp=)

# 默认目标：编译所有程序
all: $(C_BINS) $(CPP_BINS)

# C 文件的编译规则
%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# CPP 文件的编译规则
%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# 清理命令
clean:
	rm -f $(C_BINS) $(CPP_BINS)

.PHONY: all clean
