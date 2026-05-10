
#include "kvstore.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <time.h> 
#include <string.h>

#define BEGIN_TEST      0

SkipList_t global_skiptable;

/*
@ return =0 : success   , <0 : fail   , >0  :  key has existed
*/

skiptable_node_t* createNode(int level, char* key, char* value) {
    skiptable_node_t* newNode = kvs_malloc(sizeof(skiptable_node_t));
    if (newNode == NULL) return NULL;

    newNode->forward = kvs_malloc((level + 1) * sizeof(skiptable_node_t*));

    if (key != NULL) {
        newNode->key = kvs_malloc(strlen(key) + 1);
        if (newNode->key == NULL) goto fail;
        strcpy(newNode->key, key);
    } else {
        newNode->key = NULL;
    }

    if (value != NULL) {
        newNode->value = kvs_malloc(strlen(value) + 1);
        if (newNode->value == NULL) goto fail;
        strcpy(newNode->value, value);
    } else {
        newNode->value = NULL;
    }

    return newNode;

fail:
    kvs_free(newNode->key);
    kvs_free(newNode->value);
    kvs_free(newNode->forward);
    kvs_free(newNode);
    return NULL;
}


SkipList_t* createSkipList() {
    SkipList_t* skipList = (SkipList_t*)kvs_malloc(sizeof(SkipList_t));
    skipList->level = 0;
    
    skipList->header = createNode(MAX_LEVEL, NULL, NULL);
    
    for (int i = 0; i <= MAX_LEVEL; ++i) {
        skipList->header->forward[i] = NULL;
    }
    
    return skipList; 
}

int randomLevel() {
   int level = 0;
   while (rand() < RAND_MAX / 2 && level < MAX_LEVEL)
      level++;
   return level;
}

bool insert(SkipList_t* skipList, char* key, char* value) {
   skiptable_node_t* update[MAX_LEVEL + 1];
   skiptable_node_t* current = skipList->header;

   for (int i = skipList->level; i >= 0; --i) {
      while (current->forward[i] != NULL && strcmp(current->forward[i]->key , key) < 0)
         current = current->forward[i];
      update[i] = current;
   }

   current = current->forward[0];

   if (current == NULL || strcmp(current->key, key) != 0) {
      int level = randomLevel();

      if (level > skipList->level) {
         for (int i = skipList->level + 1; i <= level; ++i)
            update[i] = skipList->header;
         skipList->level = level;
      }

      skiptable_node_t* newNode = createNode(level, key, value);

      for (int i = 0; i <= level; ++i) {
         newNode->forward[i] = update[i]->forward[i];
         update[i]->forward[i] = newNode;
      }
#if BEGIN_TEST      
      printf("Inserted key %s\n", key);
#endif    
      return true;
   } else {

#if BEGIN_TEST
       printf("Key %s already exists\n", key);
#endif
       return false;
   }
}

void display(SkipList_t* skipList) {
    printf("Skip List:\n");
    
    for (int i = 0; i <= skipList->level; ++i) {
        skiptable_node_t* node = skipList->header->forward[i];
        printf("Level %d: ", i);
        
        while (node != NULL) {
            printf("%s ", node->key);
            node = node->forward[i];
        }
        
        printf("\n");
    }
}

bool search(SkipList_t* skipList, char* key) {
    skiptable_node_t* current = skipList->header;

    for (int i = skipList->level; i >= 0; --i) {
        while (current->forward[i] != NULL && strcmp(current->forward[i]->key , key) < 0)
            current = current->forward[i];
    }

    current = current -> forward[0];

    if(current && strcmp(current -> key, key) == 0){

#if BEGIN_TEST
        printf("Key %s found with value %s\n", key, current->value);
#endif
        return true;
    }else{

#if BEGIN_TEST
        printf("Key %s not found\n", key);
#endif
        return false;
    }
}


int kvs_skiptable_create(kvs_skiptable_t *skiptable) {
    if (!skiptable) return -1;
    skiptable->level = 0;
    skiptable->header = createNode(MAX_LEVEL, NULL, NULL);
    for (int i = 0; i <= MAX_LEVEL; ++i)
        skiptable->header->forward[i] = NULL;
    return 0;
}


void kvs_skiptable_destory(kvs_skiptable_t *skiptable) {
    if (!skiptable) return;

    // 沿第0层遍历（所有节点都在第0层，不会重复释放）
    skiptable_node_t *node = skiptable->header->forward[0];
    while (node != NULL) {
        skiptable_node_t *tmp = node;
        node = node->forward[0];

        kvs_free(tmp->key);      // kvs_malloc 配 kvs_free
        kvs_free(tmp->value);    

        kvs_free(tmp->forward);
        kvs_free(tmp);
    }

    // 释放 header
    kvs_free(skiptable->header->key);      // kvs_malloc 配 kvs_free
    kvs_free(skiptable->header->value); 

    kvs_free(skiptable->header->forward);
    kvs_free(skiptable->header);

    skiptable->level = 0;
    skiptable->header = NULL;
}

int kvs_skiptable_set(kvs_skiptable_t *skiptable, char *key, char *value) {
    skiptable_node_t* update[MAX_LEVEL + 1];
    skiptable_node_t* current = skiptable->header;

    for (int i = skiptable->level; i >= 0; --i) {
        while (current->forward[i] != NULL && strcmp(current->forward[i]->key , key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    if (current == NULL || strcmp(current->key, key) != 0) {
        int level = randomLevel();

        if (level > skiptable->level) {
            for (int i = skiptable->level + 1; i <= level; ++i) {
                update[i] = skiptable->header;
            }
            skiptable->level = level;
        }

        skiptable_node_t* newNode = createNode(level, key, value);

        for (int i = 0; i <= level; ++i) {
            newNode->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = newNode;
        }

#if BEGIN_TEST    
        printf("Inserted key %s\n", key);
#endif

        return 0;
    } else {

#if BEGIN_TEST
        printf("Key %s already exists\n", key);
#endif
        return 1;    // key already exists
    }
}

char * kvs_skiptable_get(kvs_skiptable_t *skiptable, char *key) {
    skiptable_node_t* current = skiptable->header;

    for (int i = skiptable->level; i >= 0; --i) {
        while (current->forward[i] != NULL && strcmp(current->forward[i]->key , key) < 0)
            current = current->forward[i];
    }

    current = current -> forward[0];

    if(current && strcmp(current -> key, key) == 0){

#if BEGIN_TEST
        printf("Key %s found with value %s\n", key, current->value);
#endif
        return current->value;
    }else{

#if BEGIN_TEST
        printf("Key %s not found\n", key);
#endif

        return NULL;  // key does not exist
    }
}


int kvs_skiptable_mod(kvs_skiptable_t *skiptable, char *key, char *value) {
    skiptable_node_t* current = skiptable->header;

    for (int i = skiptable->level; i >= 0; --i) {
        while (current->forward[i] != NULL && strcmp(current->forward[i]->key , key) < 0)
            current = current->forward[i];
    }

    current = current -> forward[0];

    if (current && strcmp(current->key, key) == 0) {
        kvs_free(current->value);                          // 释放旧值
        current->value = kvs_malloc(strlen(value) + 1);    // 分配新地
        strcpy(current->value, value);                     // 拷进去
        return 0;  // success

    } else {

#if BEGIN_TEST
        printf("key %s does not exist \n", key);
#endif
        return 1;  // key does not exist
    }
}

int kvs_skiptable_del(kvs_skiptable_t* skipList, char* key) {
    skiptable_node_t* update[MAX_LEVEL + 1];
    skiptable_node_t* current = skipList->header;

    for (int i = skipList->level; i >= 0; --i) {
        while (current->forward[i] != NULL && strcmp(current->forward[i]->key , key) < 0)
            current = current->forward[i];
        update[i] = current;
    }
    current = current->forward[0];

    if (current == NULL || strcmp(current->key, key) != 0) {

#if BEGIN_TEST
        printf("Key %s not found, can't delete\n", key);
#endif
        return 1;   // key does not exist
    }

    // 每层把前驱的 forward 指针绕过 current
    for (int i = 0; i <= skipList->level; ++i) {
        if (update[i]->forward[i] != current) break;  // 这一层及以上没有 current
        update[i]->forward[i] = current->forward[i];
    }

    kvs_free(current->key);
    kvs_free(current->value);

    kvs_free(current->forward);
    kvs_free(current);

    // 收缩：如果顶层变空了，level 下调
    while (skipList->level > 0 && skipList->header->forward[skipList->level] == NULL)
        skipList->level--;

#if BEGIN_TEST
    printf("Deleted key %s\n", key);
#endif

    return 0;   // success
}


int kvs_skiptable_exist(kvs_skiptable_t *skiptable, char *key) {
    skiptable_node_t* current = skiptable->header;

    for (int i = skiptable->level; i >= 0; --i) {
        while (current->forward[i] != NULL && strcmp(current->forward[i]->key , key) < 0)
            current = current->forward[i];
    }

    current = current -> forward[0];

    if(current && strcmp(current -> key, key) == 0) {

#if BEGIN_TEST
        printf("key : %s, value : %s \n", key, current->value);
#endif
        return 0;
    } else {

#if BEGIN_TEST
        printf("key %s does not exist \n", key);
#endif
        return 1;  // key does not exist
    }
}

void kvs_skiptable_foreach(kvs_skiptable_t *skiptable, kvs_visit_func visitor, void *userdata) {
    if (!skiptable || !visitor) return;

    // 第0层包含所有节点，沿着 forward[0] 遍历即可
    skiptable_node_t *node = skiptable->header->forward[0];
    while (node != NULL) {
        visitor(node->key, node->value, userdata);
        node = node->forward[0];
    }
}


#if BEGIN_TEST

int main() {
    srand(time(NULL));

    // 1. 创建
    int ret = kvs_skiptable_create(&global_skiplist);
    assert(ret == 0);

    // 2. 插入 (set)
    kvs_skiptable_set(&global_skiplist, "1001", "Alice");
    kvs_skiptable_set(&global_skiplist, "1003", "Bob");
    kvs_skiptable_set(&global_skiplist, "1005", "Charlie");
    kvs_skiptable_set(&global_skiplist, "1002", "David");
    kvs_skiptable_set(&global_skiplist, "1004", "Eve");

    display(&global_skiplist);


    // 3. 重复插入同一个 key
    ret = kvs_skiptable_set(&global_skiplist, "1003", "Bobby");
    assert(ret == 1);  // key already exists

    // 4. 查询
    char *val = kvs_skiptable_get(&global_skiplist, "1003");
    assert(val != NULL);
    printf("get 1003: %s\n", val);

    val = kvs_skiptable_get(&global_skiplist, "9999");
    assert(val == NULL);  // not found

    // 5. 修改
    ret = kvs_skiptable_mod(&global_skiplist, "1002", "Daviddd");
    assert(ret == 0);

    ret = kvs_skiptable_mod(&global_skiplist, "8888", "xxx");
    assert(ret == -1);  // key not found

    // 6. 存在性检查
    ret = kvs_skiptable_exist(&global_skiplist, "1002");
    assert(ret == 0);

    ret = kvs_skiptable_exist(&global_skiplist, "7777");
    assert(ret == -1);

    // 7. 删除
    ret = kvs_skiptable_del(&global_skiplist, "1004");
    assert(ret == 0);

    ret = kvs_skiptable_del(&global_skiplist, "1004");
    assert(ret == -1);  // already deleted

    // 8. 销毁
    kvs_skiptable_destory(&global_skiplist);

    printf("All tests passed!\n");
    return 0;
}

#endif




