


#include "kvstore.h"
#include "my_persist.h"

#if MEM_ALLOC_MODE == MEM_ALLOC_MEMPOOL
#include "mempool.h"
#endif


#include "my_protocol_parsing.h"



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


void *kvs_malloc(size_t size) {
#if MEM_ALLOC_MODE == MEM_ALLOC_MEMPOOL
	return mempool_alloc(size);
#elif MEM_ALLOC_MODE == MEM_ALLOC_RAW
	return malloc(size);
#elif MEM_ALLOC_MODE == MEM_ALLOC_JEMALLOC
	return malloc(size);
#endif
}

void kvs_free(void *ptr) {
#if MEM_ALLOC_MODE == MEM_ALLOC_MEMPOOL
	mempool_free(ptr);
#elif MEM_ALLOC_MODE == MEM_ALLOC_RAW
	free(ptr);
#elif MEM_ALLOC_MODE == MEM_ALLOC_JEMALLOC
	free(ptr);
#endif
}


enum {
	KVS_CMD_START = 0,
	// array
	KVS_CMD_SET = KVS_CMD_START,
	KVS_CMD_GET,
	KVS_CMD_DEL,
	KVS_CMD_MOD,
	KVS_CMD_EXIST,
	// rbtree
	KVS_CMD_RSET,
	KVS_CMD_RGET,
	KVS_CMD_RDEL,
	KVS_CMD_RMOD,
	KVS_CMD_REXIST,
	// hash
	KVS_CMD_HSET,
	KVS_CMD_HGET,
	KVS_CMD_HDEL,
	KVS_CMD_HMOD,
	KVS_CMD_HEXIST,
	// skiptable
	KVS_CMD_STSET,
	KVS_CMD_STGET,
	KVS_CMD_STDEL,
	KVS_CMD_STMOD,
	KVS_CMD_STEXIST,

	KVS_CMD_COUNT,
};



const char *command[] = {
	"SET", "GET", "DEL", "MOD", "EXIST",
	"RSET", "RGET", "RDEL", "RMOD", "REXIST",
	"HSET", "HGET", "HDEL", "HMOD", "HEXIST",
	"STSET", "STGET", "STDEL", "STMOD", "STEXIST"
};

#if 0

int kvs_split_token(char *msg, char *tokens[]) {

	if (msg == NULL || tokens == NULL) return -1;

	int idx = 0;
	char *token = strtok(msg, " ");

	while (token != NULL) {
		//printf("idx: %d, %s\n", idx, token);

		tokens[idx ++] = token;
		token = strtok(NULL, " ");
	}

	return idx;
}
#endif



#define	PRINT_TO_CONSOLE	0

#if PRINT_TO_CONSOLE
	int all_count = 0;
#endif

static void wal_log(char *tokens[]) {
	if (PERSIST_MODE == PERSIST_MODE_DISABLE) return;
	char buf[1024] = {0};   // 每次调用 wal_log 的时候, 都清零
	if (tokens[2]) {
		snprintf(buf, sizeof(buf), "%s %s %s", tokens[0], tokens[1], tokens[2]);
	} else {
		snprintf(buf, sizeof(buf), "%s %s", tokens[0], tokens[1]);
	}
	
	if (persist_append_wal_buffer(buf) == 0) {   //  ==0, 向 wal_log buffer 写入 ;  ==-1, 系统重放, 避免冲入写入 wal_log buffer
		static int write_count = 0;
		write_count++;
		if (write_count >= KVS_WAL_BATCH_SIZE) {
			persist_flush_wal();

#if PRINT_TO_CONSOLE
			all_count += write_count;
			printf("now count : %d \n", all_count);
#endif

			write_count = 0;
		}
	}
}

int kvs_filter_protocol(char **tokens, int count, char *response) {

	if (tokens[0] == NULL || count == 0 || response == NULL) return -1;

	int cmd = KVS_CMD_START;
	for (cmd = KVS_CMD_START; cmd < KVS_CMD_COUNT; cmd ++) {
		if (strcmp(tokens[0], command[cmd]) == 0) {
			break;
		}
	}

    int length = 0;
	int ret = 0;
	char *key = tokens[1];
	char *value = tokens[2];

	// 写操作（SET/DEL/MOD）需要加锁，防止与快照线程并发
	int op = cmd % 5;
	int is_write = (op == KVS_CMD_SET || op == KVS_CMD_DEL || op == KVS_CMD_MOD);
	if (PERSIST_MODE == PERSIST_MODE_ENABLE && is_write) persist_lock();

    switch(cmd) {
#if ENABLE_ARRAY
	case KVS_CMD_SET:
		ret = kvs_array_set(&global_array ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "EXIST\r\n");
		}

		break;
	case KVS_CMD_GET:
		char *result = kvs_array_get(&global_array, key);
		if (result == NULL) {
			length = sprintf(response, "NO EXIST\r\n");
		} else {
			length = sprintf(response, "%s\r\n", result);
		}
		break;

	case KVS_CMD_DEL:
		ret = kvs_array_del(&global_array ,key);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;

	case KVS_CMD_MOD:
		ret = kvs_array_mod(&global_array ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;

	case KVS_CMD_EXIST:
		ret = kvs_array_exist(&global_array ,key);
		if (ret == 0) {
			length = sprintf(response, "EXIST\r\n");
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;


#endif

	// rbtree
#if ENABLE_RBTREE
	case KVS_CMD_RSET:
		ret = kvs_rbtree_set(&global_rbtree ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "EXIST\r\n");
		}

		break;
	case KVS_CMD_RGET: {
		char *result = kvs_rbtree_get(&global_rbtree, key);
		if (result == NULL) {
			length = sprintf(response, "NO EXIST\r\n");
		} else {
			length = sprintf(response, "%s\r\n", result);
		}
		break;
	}
	case KVS_CMD_RDEL:
		ret = kvs_rbtree_del(&global_rbtree ,key);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_RMOD:
		ret = kvs_rbtree_mod(&global_rbtree ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_REXIST:
		ret = kvs_rbtree_exist(&global_rbtree ,key);
		if (ret == 0) {
			length = sprintf(response, "EXIST\r\n");
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
#endif


#if ENABLE_HASH

	case KVS_CMD_HSET:
		ret = kvs_hash_set(&global_hash ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "EXIST\r\n");
		}

		break;
	case KVS_CMD_HGET: {
		char *result = kvs_hash_get(&global_hash, key);
		if (result == NULL) {
			length = sprintf(response, "NO EXIST\r\n");
		} else {
			length = sprintf(response, "%s\r\n", result);
		}
		break;
	}
	case KVS_CMD_HDEL:
		ret = kvs_hash_del(&global_hash ,key);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_HMOD:
		ret = kvs_hash_mod(&global_hash ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");

			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);

		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_HEXIST:
		ret = kvs_hash_exist(&global_hash ,key);
		if (ret == 0) {
			length = sprintf(response, "EXIST\r\n");
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;



#endif


#if ENABLE_SKIPTABLE

	case KVS_CMD_STSET:
		ret = kvs_skiptable_set(&global_skiptable ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");
			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);
		} else {
			length = sprintf(response, "EXIST\r\n");
		}

		break;
	case KVS_CMD_STGET: {
		char *result = kvs_skiptable_get(&global_skiptable, key);
		if (result == NULL) {
			length = sprintf(response, "NO EXIST\r\n");
		} else {
			length = sprintf(response, "%s\r\n", result);
		}
		break;
	}
	case KVS_CMD_STDEL:
		ret = kvs_skiptable_del(&global_skiptable ,key);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");
			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_STMOD:
		ret = kvs_skiptable_mod(&global_skiptable ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");
			if (PERSIST_MODE == PERSIST_MODE_ENABLE) wal_log(tokens);
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_STEXIST:
		ret = kvs_skiptable_exist(&global_skiptable ,key);
		if (ret == 0) {
			length = sprintf(response, "EXIST\r\n");
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;

#endif

	default:
		assert(0);
	}

	if (PERSIST_MODE == PERSIST_MODE_ENABLE && is_write) persist_unlock();

    return length;
}



int kvs_protocol(char *msg, int length, char *response) {

// SET Key Value
// GET Key
// DEL Key

    if (msg == NULL || length <= 0 || response == NULL) return -1;

    // memcpy(response, msg, length);

    // printf("recv %d : %s\n", length, msg);

    char *tokens[KVS_MAX_TOKENS] = {0};

    int count = kvs_split_token(msg, tokens);
	if (count == -1) return -1;

    return kvs_filter_protocol(tokens, count, response);
}

int init_kvengine(void) {

#if ENABLE_ARRAY
	memset(&global_array, 0, sizeof(kvs_array_t));
	kvs_array_create(&global_array);
#endif

#if ENABLE_RBTREE
	memset(&global_rbtree, 0, sizeof(kvs_rbtree_t));
	kvs_rbtree_create(&global_rbtree);
#endif

#if ENABLE_HASH
	memset(&global_hash, 0, sizeof(kvs_hash_t));
	kvs_hash_create(&global_hash);
#endif

#if ENABLE_SKIPTABLE
	memset(&global_skiptable, 0, sizeof(kvs_skiptable_t));
	kvs_skiptable_create(&global_skiptable);

#endif

	return 0;
}


void dest_kvengine(void) {

#if ENABLE_ARRAY
	kvs_array_destory(&global_array);
#endif

#if ENABLE_RBTREE
	kvs_rbtree_destory(&global_rbtree);
#endif

#if ENABLE_HASH
	kvs_hash_destory(&global_hash);
#endif

#if ENABLE_SKIPTABLE
	kvs_skiptable_destory(&global_skiptable);
#endif

}



int main(int argc, char* argv[]) {

    if (argc != 2) return -1;

    int port = atoi(argv[1]);

#if MEM_ALLOC_MODE == MEM_ALLOC_MEMPOOL
    mempool_init();
#endif
    init_kvengine();

#if MEM_ALLOC_MODE == MEM_ALLOC_MEMPOOL
    printf("memory allocator: MEM_ALLOC_MEMPOOL\n");
#elif MEM_ALLOC_MODE == MEM_ALLOC_RAW
    printf("memory allocator: MEM_ALLOC_RAW\n");
#elif MEM_ALLOC_MODE == MEM_ALLOC_JEMALLOC
    printf("memory allocator: MEM_ALLOC_JEMALLOC\n");
#endif

	if (PERSIST_MODE == PERSIST_MODE_ENABLE) persist_init(kvs_protocol);

#if (NETWORK_SELECT == NETWORK_REACTOR)
	reactor_start(port, kvs_protocol);  //
#elif (NETWORK_SELECT == NETWORK_PROACTOR)
	proactor_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_NTYCO)
	ntyco_start(port, kvs_protocol);
#endif

	if (PERSIST_MODE == PERSIST_MODE_ENABLE) persist_close();

    dest_kvengine();
#if MEM_ALLOC_MODE == MEM_ALLOC_MEMPOOL
    mempool_destroy();
#endif

}
