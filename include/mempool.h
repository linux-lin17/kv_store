#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <stdlib.h>

void mempool_init(void);
void mempool_destroy(void);

void *mempool_alloc(size_t size);
void  mempool_free(void *ptr);

#endif
