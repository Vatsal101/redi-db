#ifndef INDEX_H
#define INDEX_H 

#include <stdint.h>  // For uint32_t, uint8_t, uint16_t
#include <stddef.h>  // For size_t
#include <stdio.h>   // For FILE*

typedef struct {
    uint8_t tombstone; // 0 = put, 1 = tombstone
	uint32_t offset;  // actual offsetvalue 
} hash_table_val;



int insert(const char *key, const char *value);
int get(const char *path);
int remove(const char *key);
#endif

