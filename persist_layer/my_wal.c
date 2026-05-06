#include "my_persist.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#define WAL_FILE "persist_layer/appendonly.wal"
#define SNAPSHOT_INTERVAL_SEC 10

static FILE *wal_fp = NULL;

// 重放中标记，防止重放时再次往 WAL 里写（死循环）
static int wal_replaying = 0;

// 快照定时器线程
static pthread_t snapshot_thread;

static volatile int snapshot_stop = 0;    // 0 : work  , 1 : stop

// 快照与写操作协调锁
static pthread_mutex_t persist_mutex = PTHREAD_MUTEX_INITIALIZER;

// ==================== 锁接口（供核心层使用）====================

void persist_lock(void) {
    pthread_mutex_lock(&persist_mutex);
}

void persist_unlock(void) {
    pthread_mutex_unlock(&persist_mutex);
}

// ==================== WAL 截断（快照后清空 WAL）====================

static void _wal_truncate(void) {
    if (wal_fp) {
        fclose(wal_fp);
    }
    // 用 "w" 打开再关闭 → 截断为空文件
    wal_fp = fopen(WAL_FILE, "w");
    if (wal_fp) {
        fclose(wal_fp);
    }
    // 重新以追加模式打开
    wal_fp = fopen(WAL_FILE, "a");
    if (!wal_fp) {
        perror("[WAL] truncate reopen");
        return;
    }
    setvbuf(wal_fp, NULL, _IOLBF, 0);
    printf("[WAL] truncated after snapshot\n");
}

// ==================== 快照定时器线程 ====================

static void *_snapshot_timer(void *arg) {
    while (!snapshot_stop) {
        sleep(SNAPSHOT_INTERVAL_SEC);
        if (snapshot_stop) break;

        persist_lock();

        snapshot_dump();  // k-v ---> dump.rdb

        _wal_truncate();  // 清空 wal log

        persist_unlock();
    }
    return NULL;
}

// ==================== 持久化层生命周期 ====================

int persist_init(kvs_protocol_t kvs_protocol) {

    mkdir("persist_layer", 0755);

    // --- 第一步：将 snapshot 中的数据载入 ---
    snapshot_load();

    // --- 第二步：重放已有 WAL（如果存在） ---
    FILE *fp = fopen(WAL_FILE, "r");
    if (fp) {

        wal_replaying = 1;

        printf("[WAL] replaying existing log...\n");

        char line[1024];
        int count = 0;
        while (fgets(line, sizeof(line), fp)) {
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
            if (len > 1 && line[len - 2] == '\r') line[len - 2] = '\0';

            if (line[0] == '\0') continue;

            char resp[1024] = {0};
            kvs_protocol(line, strlen(line), resp);
            count++;
        }
        fclose(fp);
        wal_replaying = 0;
        printf("[WAL] replay done, %d commands recovered\n", count);
    }

    // --- 第三步：打开 WAL 文件（追加写入） ---
    wal_fp = fopen(WAL_FILE, "a");
    if (!wal_fp) {
        perror("[WAL] fopen");
        return -1;
    }
    setvbuf(wal_fp, NULL, _IOLBF, 0);

    // --- 第四步：启动快照定时器线程 ---
    snapshot_stop = 0;
    if (pthread_create(&snapshot_thread, NULL, _snapshot_timer, NULL) != 0) {
        perror("[WAL] pthread_create snapshot timer");
    }

    printf("[WAL] ready, writing to %s\n", WAL_FILE);
    return 0;
}

int persist_append_wal(const char *cmd) {
    if (!wal_fp || !cmd || wal_replaying) return -1;

    fprintf(wal_fp, "%s\n", cmd);
    fflush(wal_fp);

    return 0;
}

void persist_close(void) {
    // 停止定时器
    snapshot_stop = 1;
    pthread_join(snapshot_thread, NULL);

    // 最后一次快照
    snapshot_dump();   // k-v ---> dump.rdb
    _wal_truncate();   // 清空 wal log

    if (wal_fp) {
        fclose(wal_fp);
        wal_fp = NULL;
        printf("[WAL] closed\n");
    }
}
