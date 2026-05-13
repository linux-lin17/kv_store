#ifndef __MY_PERSIST_H__
#define __MY_PERSIST_H__


typedef int (*kvs_protocol_t)(char *msg, int length, char *response);

// ==================== 对外接口（核心层可调用）====================

// 初始化持久化层：加载快照 → 重放 WAL → 打开 WAL 追加写入 → 启动快照定时器
// 必须在 init_kvengine() 之后调用（引擎已就绪才能恢复数据）
int persist_init(kvs_protocol_t kvs_protocol);

/*追加一条命令到 WAL 日志的缓冲区, （仅操作 SET/DEL/MOD 调用）
/ persist_flush_wal() 之后, 真正进入 wal log 
/ return  : ==0, 向 wal buffer 写入成功 ;  ==-1, 系统重放, 或 cmd == NULL, 或 wal log 不存在
*/
int persist_append_wal_buffer(const char *cmd);

// 批量刷盘：将缓冲区中的 WAL 数据一次性写入内核（批量模式下由核心层攒够一批后调用）
void persist_flush_wal(void);

// 关闭持久化层：停止定时器 → 最后一次快照 → 关闭 WAL，在 dest_kvengine() 之前调用
void persist_close(void);


// ========== 快照协调锁（核心层写操作前加锁，写操作后解锁）==========

void persist_lock(void);
void persist_unlock(void);


// ========== 以下接口仅供 persist 层内部使用，禁止外部调用 ==========

// k-v ---> dump.rdb（由定时器线程调用）
int snapshot_dump(void);

// dump.rdb ---> k-v（由 persist_init 内部调用）
int snapshot_load(void);

#endif