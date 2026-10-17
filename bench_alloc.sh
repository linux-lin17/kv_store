#!/usr/bin/env bash
# =============================================================
# 分配器对比实验：
#   raw      (不链 jemalloc)  => glibc malloc
#   jemalloc (链 -ljemalloc)  => jemalloc 通过符号抢占接管 malloc
#   mempool  (不链 jemalloc)  => 自写 slab 内存池（页来自 glibc）
#
# 用法: ./bench_alloc.sh [PORT] [RUNS]
#   PORT  服务端口，默认 19001
#   RUNS  每个配置重复轮数，默认 3（每轮重启服务端，避免残留状态影响）
#
# 环境变量 CLIENT_MODE 可换测试用例（默认 4 = skiptable_testcase_1w）
# =============================================================
set -u

PORT=${1:-19001}
RUNS=${2:-3}
CLIENT_MODE=${CLIENT_MODE:-4}

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR" || exit 1

RESULT_DIR=bench_results
mkdir -p "$RESULT_DIR"

# 格式: 配置名|make 参数
CONFIGS="raw|MEM_MODE=raw mempool|MEM_MODE=mempool jemalloc|MEM_MODE=jemalloc"

echo "== 分配器对比实验: port=$PORT, runs=$RUNS, client_mode=$CLIENT_MODE =="

for cfg in $CONFIGS; do
    name=${cfg%%|*}
    args=${cfg#*|}

    echo
    echo "===== [$name] 构建: make $args ====="
    make clean >/dev/null
    if ! make $args >/dev/null; then
        echo "[$name] 构建失败"
        exit 1
    fi

    outfile="$RESULT_DIR/qps-$name.txt"
    : > "$outfile"

    for run in $(seq 1 "$RUNS"); do
        log="$RESULT_DIR/server-$name-run$run.log"
        # JEMALLOC_STATS=1: jemalloc 档在 SIGINT 时把完整统计写入日志（其余档忽略该变量）
        JEMALLOC_STATS=1 ./kvstore "$PORT" > "$log" 2>&1 &
        pid=$!

        # banner 在 init_kvengine 加载完数据后才打印，出现即视为就绪（最多 15s）
        for _ in $(seq 1 150); do
            grep -q "memory allocator" "$log" 2>/dev/null && break
            sleep 0.1
        done
        # 横幅在 listen 之前打印，连不上时重试几次
        result=""
        for _ in $(seq 1 10); do
            result=$(./my_testcase 127.0.0.1 "$PORT" "$CLIENT_MODE" 2>&1)
            echo "$result" | grep -q "qps" && break
            sleep 0.3
        done

        echo "$result" | grep "qps" | sed "s/^/Run $run: /" >> "$outfile"
        cat "$outfile"

        # SIGINT 停服：jemalloc 版本会顺带把统计打印进日志
        kill -INT "$pid" 2>/dev/null
        sleep 0.5
        if kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null
        fi
        wait "$pid" 2>/dev/null
    done
done

echo
echo "== 汇总 =="
for f in "$RESULT_DIR"/qps-*.txt; do
    [ -f "$f" ] || continue
    printf "%-18s " "$(basename "$f")"
    grep -oE "qps: [0-9]+" "$f" | grep -oE "[0-9]+" |
        awk '{s+=$1; n++; if(min==0||$1<min)min=$1; if($1>max)max=$1}
             END {if(n) printf "runs=%d avg=%.0f min=%d max=%d", n, s/n, min, max; else printf "no data"}'
    echo
done
echo "明细见 $RESULT_DIR/qps-*.txt，服务端日志见 $RESULT_DIR/server-*.log"
