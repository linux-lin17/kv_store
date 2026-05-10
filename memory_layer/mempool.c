#include "mempool.h"
#include <string.h>

/* ==================== 挡位定义 ==================== */

#define SLAB_32    0
#define SLAB_64    1
#define SLAB_128   2
#define SLAB_256   3

#define SLAB_COUNT 4

#define FALLBACK   (-1)

#define PAGE_SIZE  4096

/* ==================== 数据结构 ==================== */

// 每个 chunk 的前置头（16 字节），kvs_free 时反向读取以确定归还到哪个 slab
typedef struct chunk_header {
    struct chunk_header *next;
    int                 slab_index;
} chunk_header_t;

#define HEADER_SIZE  ((int)sizeof(chunk_header_t))

// 每个 page 前 8 字节用作 page 链表节点，destroy 时遍历释放
typedef struct page_link {
    struct page_link *next;
} page_link_t;

// 一个 slab 档位的管理者
typedef struct slab_s {
    chunk_header_t *free_list;    // 空闲 chunk 链表
    page_link_t    *page_list;    // 已申请 page 链表（destroy 用）
    size_t          chunk_size;   // HEADER_SIZE + usable_size，单个 chunk 总长
    size_t          usable_size;  // 调用方可见的可用字节数
} slab_t;

static slab_t slabs[SLAB_COUNT]; 

/* ==================== 内部：为新 slab 申请一个 page 并切分成 chunk ==================== */

static void slab_grow(slab_t *slab) {
    char *raw = (char *)malloc(PAGE_SIZE);
    if (!raw) return;

    // 前 8 字节串入 page 链表
    page_link_t *pl = (page_link_t *)raw;
    pl->next = slab->page_list;
    slab->page_list = pl;

    // 剩余空间全部切成固定大小的 chunk
    char *area = raw + sizeof(page_link_t);
    int n = (int)((PAGE_SIZE - sizeof(page_link_t)) / slab->chunk_size);

    for (int i = 0; i < n; i++) {
        chunk_header_t *chunk = (chunk_header_t *)(area + i * slab->chunk_size);
        chunk->slab_index = (int)(slab - slabs);    // 确定好是哪一个分组   0/1/2/3
        chunk->next       = slab->free_list;
        slab->free_list   = chunk;
    }
}

/* ==================== 对外接口 ==================== */

void mempool_init() {
    
    memset(slabs, 0, sizeof(slabs));

    // 4 档：usable 32 / 64 / 128 / 256，含 header 后为实际 chunk 大小
    slabs[SLAB_32].usable_size  = 32;
    slabs[SLAB_64].usable_size  = 64;
    slabs[SLAB_128].usable_size = 128;
    slabs[SLAB_256].usable_size = 256;

    for (int i = 0; i < SLAB_COUNT; i++) {
        slabs[i].chunk_size = HEADER_SIZE + slabs[i].usable_size;
    }
}

void mempool_destroy() {
    for (int i = 0; i < SLAB_COUNT; i++) {
        page_link_t *page = slabs[i].page_list;
        while (page) {
            page_link_t *next = page->next;
            free(page);
            page = next;
        }
        slabs[i].page_list = NULL;
        slabs[i].free_list = NULL;
    }
}

void *mempool_alloc(size_t size) {
    if (size == 0) return NULL;

    // 根据 size 路由到对应 slab 档位
    int idx;
    if (size <= 32)
        idx = SLAB_32;
    else if (size <= 64)
        idx = SLAB_64;
    else if (size <= 128)
        idx = SLAB_128;
    else if (size <= 256)
        idx = SLAB_256;
    else {
        // fallback：大对象直接用 malloc，但前面多分配一个 header，
        // 标记 slab_index = -1，kvs_free 才能统一释放
        chunk_header_t *hdr = (chunk_header_t *)malloc(HEADER_SIZE + size);
        if (!hdr) return NULL;
        hdr->slab_index = FALLBACK;
        return (void *)(hdr + 1);
    }

    slab_t *slab = &slabs[idx];

    // 空闲链表为空则先扩容
    if (!slab->free_list)
        slab_grow(slab);

    // 从空闲链表头部摘取一个 chunk
    chunk_header_t *chunk = slab->free_list;
    if (!chunk) return NULL;          // OOM：malloc PAGE_SIZE 失败

    slab->free_list = chunk->next;
    return (void *)(chunk + 1);
}

void mempool_free(void *ptr) {

    if (!ptr) return;

    // 反向读取 header
    chunk_header_t *hdr = (chunk_header_t *)ptr - 1;

    if (hdr->slab_index == FALLBACK) {
        free(hdr);
        return;
    }

    slab_t *slab = &slabs[hdr->slab_index];

    // 头插法归还到空闲链表
    hdr->next      = slab->free_list;
    slab->free_list = hdr;
}