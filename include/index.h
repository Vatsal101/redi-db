#ifndef INDEX_H
#define INDEX_H 

#include <stdint.h>  // For uint32_t, uint8_t, uint16_t
#include <stddef.h>  // For size_t
#include <stdio.h>   // For FILE*
#include <string.h>

typedef struct {
	uint8_t tombstone; // 0 = put, 1 = tombstone
	long offset;  // actual offsetvalue 
	char* key; // value of the key
} hash_table_val;

int init_hash_table(void);
void cleanup_hash_table(void);
unsigned long hash(const char *str);
void resize(void);
int get(const char *key);
int insert(const char *key, long value);
int delete(const char *key);

extern hash_table_val *arr_ptr;
extern int starting_elements;
extern int size;
extern int capacity;

#endif

