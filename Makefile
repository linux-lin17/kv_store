# 指定编译器
CC = gcc

# 编译选项
CFLAGS = -Wall -g -I./include -I./NtyCo/core/ -I./persist_layer

# 链接库路径和选项（jemalloc 是否链接由下方分配器选择自动决定）
LDFLAGS = -L./NtyCo/
LDLIBS = -lntyco -lpthread -luring -ldl

# ================= 分配器选择 =================
# 分配器的选择在 include/kvstore.h 的 MEM_ALLOC_MODE 中完成，make 会自动
# 读取头文件：选中 MEM_ALLOC_JEMALLOC 时自动链接 -ljemalloc，其余两档不链。
# 也可临时用 make MEM_MODE=raw|jemalloc|mempool 覆盖头文件（优先级更高），
# bench_alloc.sh 即用该方式跑对比矩阵。
#
# （MEM_ALLOC_RAW 与 MEM_ALLOC_JEMALLOC 分支代码相同，真正的分配器差异
#  来自是否链接 -ljemalloc 的符号抢占。）
MEM_MODE ?=
JEMALLOC  ?= 0

ifneq ($(MEM_MODE),)
# 命令行覆盖头文件
CFLAGS += -DMEM_ALLOC_MODE=MEM_ALLOC_$(shell echo $(MEM_MODE) | tr 'a-z' 'A-Z')
ifeq ($(MEM_MODE),jemalloc)
JEMALLOC := 1
endif
else
# 跟随头文件：grep 出 kvstore.h 里当前的 MEM_ALLOC_MODE
HEADER_MODE := $(shell grep -hE '^\s*#\s*define\s+MEM_ALLOC_MODE\s+' include/kvstore.h | grep -oE 'MEM_ALLOC_(MEMPOOL|RAW|JEMALLOC)')
ifeq ($(HEADER_MODE),MEM_ALLOC_JEMALLOC)
JEMALLOC := 1
endif
endif

ifeq ($(JEMALLOC),1)
CFLAGS += -DUSE_JEMALLOC=1
LDLIBS += -ljemalloc
endif


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
.PHONY: all clean ntyco FORCE

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
# FLAG_STAMP 记录编译开关：切换 MEM_MODE / JEMALLOC 后自动强制重编，
# 避免 make 感知不到 -D 变化而残留旧模式的对象文件
FORCE:
FLAG_STAMP = build/.cflags_stamp
$(FLAG_STAMP): FORCE
	@mkdir -p build
	@if [ ! -f $@ ] || [ "$$(cat $@)" != "$(CFLAGS)" ]; then echo "$(CFLAGS)" > $@; fi

build/%.o: %.c $(FLAG_STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# 自动跟踪头文件依赖（-MMD 生成的 .d 文件）：改 kvstore.h 等头文件后
# 直接 make 即可，无需 make clean
-include $(KV_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

# 清理
clean:
	rm -rf build $(TARGET) $(TEST_TARGET)
	$(MAKE) -C ./NtyCo clean
