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

typedef struct {
	hash_table_val *arr_ptr;
	int starting_elements;
	int size;
	int capacity;
} HashTable;


int init_hash_table(HashTable *ht);
void cleanup_hash_table(HashTable *ht);
unsigned long hash(const char *str);
void resize(HashTable *ht);
long get(HashTable *ht, const char *key);
int insert(HashTable *ht, const char *key, long value);
int delete(HashTable *ht, const char *key);

// extern hash_table_val *arr_ptr;
// extern int starting_elements;
// extern int size;
// extern int capacity;

#endif

