
#include "kvstore.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define SNAPSHOT_FILE   "persist_layer/dump.rdb"
#define WAL_FILE        "persist_layer/appendonly.wal"

#if ENABLE_ARRAY
extern kvs_array_t global_array;
#endif

#if ENABLE_RBTREE
extern kvs_rbtree_t global_rbtree;
#endif

#if ENABLE_HASH
extern kvs_hash_t global_hash;
#endif

#if ENABLE_SKIPTABLE
extern kvs_skiptable_t global_skiptable;
#endif

// 写回调：每遇到一个 K-V 对就写一行到 dump 文件   
static void _dump_visitor(const char *key, const char *value, void *userdata) {
    FILE *fp = (FILE *)userdata;
    fprintf(fp, "%s %s\n", key, value);
}

// ==================== 写快照 ====================

//  k-v ---> dump.rdb
int snapshot_dump() {

    mkdir("persist_layer", 0755);

    FILE *fp = fopen(SNAPSHOT_FILE, "w");
    if (!fp) {
        perror("[SNAPSHOT] fopen dump");
        return -1;
    }

#if ENABLE_ARRAY
    fprintf(fp, "# array\n");
    kvs_array_foreach(&global_array, _dump_visitor, fp);
#endif

#if ENABLE_RBTREE
    fprintf(fp, "# rbtree\n");
    kvs_rbtree_foreach(&global_rbtree, _dump_visitor, fp);
#endif

#if ENABLE_HASH
    fprintf(fp, "# hash\n");
    kvs_hash_foreach(&global_hash, _dump_visitor, fp);
#endif

#if ENABLE_SKIPTABLE
    fprintf(fp, "# skiptable\n");
    kvs_skiptable_foreach(&global_skiptable, _dump_visitor, fp);
#endif

    fclose(fp);
    printf("[SNAPSHOT] dumped to %s\n", SNAPSHOT_FILE);
    return 0;
}

// ==================== 加载快照 ====================

// dump.rdb ---> k-v
int snapshot_load() {

    FILE *fp = fopen(SNAPSHOT_FILE, "r");
    if (!fp) {
        // 首次启动还没有快照文件，不是错误
        return 0;
    }

    printf("[SNAPSHOT] loading %s ...\n", SNAPSHOT_FILE);

    int engine = -1;  // 0=array 1=rbtree 2=hash 3=skiptable
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (len > 1 && line[len - 2] == '\r') line[len - 2] = '\0';

        if (line[0] == '\0') continue;

        // 分区标记
        if (strcmp(line, "# array") == 0)      { engine = 0; continue; }
        if (strcmp(line, "# rbtree") == 0)     { engine = 1; continue; }
        if (strcmp(line, "# hash") == 0)       { engine = 2; continue; }
        if (strcmp(line, "# skiptable") == 0)  { engine = 3; continue; }

        // 解析 key value（value 可能为空）
        char *key = strtok(line, " ");
        char *value = strtok(NULL, "");
        if (!key) continue;

        switch (engine) {

#if ENABLE_ARRAY
        case 0: kvs_array_set(&global_array, key, value ? value : ""); break;
#endif

#if ENABLE_RBTREE
        case 1: kvs_rbtree_set(&global_rbtree, key, value ? value : ""); break;
#endif

#if ENABLE_HASH
        case 2: kvs_hash_set(&global_hash, key, value ? value : ""); break;
#endif

#if ENABLE_SKIPTABLE
        case 3: kvs_skiptable_set(&global_skiptable, key, value ? value : ""); break;
#endif

        default: break;
        }
        count++;
    }

    fclose(fp);
    printf("[SNAPSHOT] loaded %d entries\n", count);
    return 0;
}
