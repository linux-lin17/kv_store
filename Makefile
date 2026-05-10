# 指定编译器
CC = gcc

# 编译选项
CFLAGS = -Wall -g -I./include -I./NtyCo/core/ -I./persist_layer

# 链接库路径和选项
LDFLAGS = -L./NtyCo/
LDLIBS = -lntyco -lpthread -luring -ldl -ljemalloc


# ================= 核心改动在这里 =================

# 1. 抽离出"公共代码"（不带 main 函数的底层组件）
COMMON_SRCS = network_layer/my_ntyco.c \
              network_layer/my_proactor.c \
              network_layer/my_reactor.c \
              engine_layer/my_kvs_array.c \
              engine_layer/my_kvs_rbtree.c \
              engine_layer/my_kvs_hash.c \
              engine_layer/my_kvs_skiptable.c \
              protocol_layer/my_protocol_parsing.c \
              persist_layer/my_wal.c \
              persist_layer/my_snapshot.c \
              memory_layer/mempool.c

# 2. kvstore 服务端专属代码（公共代码 + kvstore.c）
KV_SRCS = core_layer/kvstore.c $(COMMON_SRCS)
KV_OBJS = $(patsubst %.c, build/%.o, $(KV_SRCS))

# 3. testcase 测试专属代码
TEST_SRCS = my_testcase.c
TEST_OBJS = $(patsubst %.c, build/%.o, $(TEST_SRCS))

# 最终生成的两个可执行程序名字
TARGET = kvstore
TEST_TARGET = my_testcase

# 伪目标声明
.PHONY: all clean ntyco

# 默认目标：把 ntyco库、kvstore服务端、testcase客户端 全部编译出来！
all: ntyco $(TARGET) $(TEST_TARGET)

# 编译 NtyCo 库
ntyco:
	$(MAKE) -C ./NtyCo

# 组装 kvstore 程序
$(TARGET): $(KV_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# 组装 testcase 程序
$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# 统一的 .o 文件生成规则
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -rf build $(TARGET) $(TEST_TARGET)
	$(MAKE) -C ./NtyCo clean
