#ifndef __KV_STORE_H__
#define __KV_STORE_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

#define NETWORK_REACTOR 	0
#define NETWORK_PROACTOR	1
#define NETWORK_NTYCO		2

#define NETWORK_SELECT		NETWORK_NTYCO

#define ENABLE_ARRAY		1
#define ENABLE_RBTREE		1
#define ENABLE_HASH			1
#define ENABLE_SKIPTABLE	1

#define KVS_MAX_TOKENS		128


// ---------------------- 持久化模式 -----------------------

#define	PERSIST_MODE_DISABLE  	0
#define	PERSIST_MODE_ENABLE   	1

#define PERSIST_MODE  PERSIST_MODE_ENABLE

// ---------------------- 内存分配器选择 --------------------

#define	MEM_ALLOC_MEMPOOL    	11 // 自定义内存池
#define	MEM_ALLOC_RAW        	12 // 原生 malloc/free
#define	MEM_ALLOC_JEMALLOC 		13  // jemalloc

#define MEM_ALLOC_MODE  MEM_ALLOC_RAW

void *kvs_malloc(size_t size);
void kvs_free(void *ptr);

// ----------------- 批量向 wal log 写入的个数 --------------

#define KVS_WAL_BATCH_SIZE  10


// --------------------------------------------------------


// 遍历引擎 K-V 对的回调：每发现一个 key-value 就调用一次
typedef void (*kvs_visit_func)(const char *key, const char *value, void *userdata);

typedef int (*msg_handler)(char *msg, int length, char *response);

extern int reactor_start(unsigned short port, msg_handler handler);
extern int proactor_start(unsigned short port, msg_handler handler);
extern int ntyco_start(unsigned short port, msg_handler handler);



#if ENABLE_ARRAY

typedef struct kvs_array_item_s {
	char *key;
	char *value;
} kvs_array_item_t;

#define KVS_ARRAY_SIZE		1024

typedef struct kvs_array_s {
	kvs_array_item_t *table;
	int idx;
	int total;
} kvs_array_t;

int kvs_array_create(kvs_array_t *inst);
void kvs_array_destory(kvs_array_t *inst);

int kvs_array_set(kvs_array_t *inst, char *key, char *value);
char* kvs_array_get(kvs_array_t *inst, char *key);
int kvs_array_del(kvs_array_t *inst, char *key);
int kvs_array_mod(kvs_array_t *inst, char *key, char *value);
int kvs_array_exist(kvs_array_t *inst, char *key);

void kvs_array_foreach(kvs_array_t *inst, kvs_visit_func visitor, void *userdata);


#endif

#if ENABLE_RBTREE

#define RED				1
#define BLACK 			2

#define ENABLE_KEY_CHAR		1

#if ENABLE_KEY_CHAR
typedef char* KEY_TYPE;
#else
typedef int KEY_TYPE; // key
#endif

typedef struct _rbtree_node {
	unsigned char color;
	struct _rbtree_node *right;
	struct _rbtree_node *left;
	struct _rbtree_node *parent;
	KEY_TYPE key;
	void *value;
} rbtree_node;

typedef struct _rbtree {
	rbtree_node *root;
	rbtree_node *nil;
} rbtree;



typedef struct _rbtree kvs_rbtree_t;

int kvs_rbtree_create(kvs_rbtree_t *inst);
void kvs_rbtree_destory(kvs_rbtree_t *inst);

int kvs_rbtree_set(kvs_rbtree_t *inst, char *key, char *value);
char* kvs_rbtree_get(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_del(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_mod(kvs_rbtree_t *inst, char *key, char *value);
int kvs_rbtree_exist(kvs_rbtree_t *inst, char *key);

void kvs_rbtree_foreach(kvs_rbtree_t *inst, kvs_visit_func visitor, void *userdata);

#endif

#if ENABLE_HASH

#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512
#define MAX_TABLE_SIZE	1024

#define ENABLE_KEY_POINTER	1


typedef struct hashnode_s {
#if ENABLE_KEY_POINTER
	char *key;
	char *value;
#else
	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];
#endif
	struct hashnode_s *next;
	
} hashnode_t;


typedef struct hashtable_s {

	hashnode_t **nodes; //* change **, 

	int max_slots;
	int count;

} hashtable_t;

typedef struct hashtable_s kvs_hash_t;


int kvs_hash_create(kvs_hash_t *hash);
void kvs_hash_destory(kvs_hash_t *hash);
int kvs_hash_set(hashtable_t *hash, char *key, char *value);
char * kvs_hash_get(kvs_hash_t *hash, char *key);
int kvs_hash_mod(kvs_hash_t *hash, char *key, char *value);
int kvs_hash_del(kvs_hash_t *hash, char *key);
int kvs_hash_exist(kvs_hash_t *hash, char *key);

void kvs_hash_foreach(kvs_hash_t *hash, kvs_visit_func visitor, void *userdata);


#endif

 
#if ENABLE_SKIPTABLE

#define MAX_LEVEL 6

typedef struct skiptable_node_s {
    char* key;
    char* value;
    struct skiptable_node_s** forward;
} skiptable_node_t;

typedef struct SkipList_s {
    int level;
    skiptable_node_t* header;
} SkipList_t;

typedef struct SkipList_s kvs_skiptable_t;

int kvs_skiptable_create(kvs_skiptable_t *skiptable);
void kvs_skiptable_destory(kvs_skiptable_t *skiptable);

int kvs_skiptable_set(kvs_skiptable_t *skiptable, char *key, char *value);
char * kvs_skiptable_get(kvs_skiptable_t *skiptable, char *key);
int kvs_skiptable_mod(kvs_skiptable_t *skiptable, char *key, char *value);
int kvs_skiptable_del(kvs_skiptable_t *skiptable, char *key);
int kvs_skiptable_exist(kvs_skiptable_t *skiptable, char *key);

void kvs_skiptable_foreach(kvs_skiptable_t *skiptable, kvs_visit_func visitor, void *userdata);


#endif 





#endif