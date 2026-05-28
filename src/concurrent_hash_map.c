#include "concurrent_hash_map.h"

int init_concurrent_hash_table(ConcurrentHashTable *cht) {
    if (!cht) return -1;
    cht->starting_buckets = 64;
    cht->bucket_ptr = calloc(cht->starting_buckets, sizeof(Bucket));
    if (!cht->bucket_ptr) return -1;
    cht->capacity = cht->starting_buckets;

    // need t intialize the hash table for each bucket and the lock for each bucket
    for (int i = 0; i < cht->capacity; i++) {
        pthread_rwlock_init(&cht->bucket_ptr[i].lock, NULL);
        cht->bucket_ptr[i].map = malloc(sizeof(HashTable));
        init_hash_table(cht->bucket_ptr[i].map); 
    }

    return 0;
}

// safely destroys the hash table and freeing all the strings in the hash table
void cleanup_concurrent_hash_table(ConcurrentHashTable *cht) {
    if (cht->bucket_ptr) {

        for (int i = 0; i < cht->capacity; i++) {

            if (cht->bucket_ptr[i].map) {
                cleanup_hash_table(cht->bucket_ptr[i].map);
                free(cht->bucket_ptr[i].map);
                pthread_rwlock_destroy(&cht->bucket_ptr[i].lock);
            }
        }

        free(cht->bucket_ptr);
        cht->bucket_ptr = NULL;
    }
   cht->capacity = cht->starting_buckets;
}


// gets the hashes for the string
unsigned long vhash(const char *s) {
    if (!s) return 0;

    unsigned long h = 0x9e3779b97f4a7c15ULL; // seed
    while (*s) {
        h ^= (unsigned char)(*s++);
        h *= 0xbf58476d1ce4e5b9ULL;
        h ^= h >> 27;
    }

    // final avalanche mix
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;

    return h;
}

int bucket_id(ConcurrentHashTable *cht, const char *s) {
    return vhash(s) % cht->capacity;
}

long concurrent_get(ConcurrentHashTable *cht, const char *key) {
    if (!cht || !key || !cht->bucket_ptr) return -1;

    int b_id = bucket_id(cht, key);

    Bucket *bucket_ptr = cht->bucket_ptr;

    pthread_rwlock_rdlock(&bucket_ptr[b_id].lock);

    long result = get(bucket_ptr[b_id].map, key);

    if (result == -1) {
        pthread_rwlock_unlock(&bucket_ptr[b_id].lock);
        return -1;
    }

    pthread_rwlock_unlock(&bucket_ptr[b_id].lock);

    return result;
}

int concurrent_insert(ConcurrentHashTable *cht, const char *key, long value) {
    if (!cht || !key || value < 0 || !cht->bucket_ptr) return -1;

    int b_id = bucket_id(cht, key);

    Bucket *bucket_ptr = cht->bucket_ptr;
    pthread_rwlock_wrlock(&bucket_ptr[b_id].lock);

    int res = insert(bucket_ptr[b_id].map, key, value);

    if (res == -1) {
        // insert failed
        pthread_rwlock_unlock(&bucket_ptr[b_id].lock);
        return -1;
    }

    pthread_rwlock_unlock(&bucket_ptr[b_id].lock);
    // successful insert
    return 0;
}

int concurrent_delete(ConcurrentHashTable *cht, const char *key) {
    if (!cht || !key || !cht->bucket_ptr) return -1;

    int b_id = bucket_id(cht, key);
    Bucket *bucket_ptr = cht->bucket_ptr;

    pthread_rwlock_wrlock(&bucket_ptr[b_id].lock);

    int res = delete(bucket_ptr[b_id].map, key);

    if (res == -1) {
        // delete failed
        pthread_rwlock_unlock(&bucket_ptr[b_id].lock);
        return -1;
    }
    pthread_rwlock_unlock(&bucket_ptr[b_id].lock);

    // delete successful  
    return 0;

}