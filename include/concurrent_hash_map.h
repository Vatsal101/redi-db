#ifndef C_HASH_MAP_H
#define C_HASH_MAP_H

#include "pthread.h"
#include "index.h"

typedef struct{
    HashTable *map;
    pthread_rwlock_t lock;
} Bucket;

typedef struct {
    Bucket *bucket_ptr;
    int starting_buckets;
    int capacity;
} ConcurrentHashTable;

int init_concurrent_hash_table(ConcurrentHashTable *cht);
void cleanup_concurrent_hash_table(ConcurrentHashTable *cht);
unsigned long vhash(const char *str);

void concurrent_resize(ConcurrentHashTable *cht);
int concurrent_get(ConcurrentHashTable *cht, char *key);

int concurrent_insert(ConcurrentHashTable *cht, const char *key, long value);
int concurrent_delete(ConcurrentHashTable *cht, const char *key);

#endif