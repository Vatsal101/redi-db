#include <stdio.h>
#include <stdlib.h>
#include "index.h"

hash_table_val *arr_ptr;
int starting_elements = 31;
int size = 0;
int capacity = 31;

int init_hash_table() {
    arr_ptr = calloc(starting_elements, sizeof(hash_table_val))
    if (!arr_ptr) return -1;
    return 0;
}

void cleanup_hash_table() {
    if (arr_ptr) {
        for (int i = 0; i < capacity; i++) {
                if (arr_ptr[i].key) {
                    free(arr_ptr[i].key);
                }
        }
        free(arr_ptr);
        arr_ptr = NULL;
    }
}


unsigned long hash(const char *str) {
    unsigned long hash = 5381; 
    int c;
    // Iterate through the string until the null terminator ('\0') is reached
    while ((c = *str++)) { 
        // A common multiplication and addition/XOR pattern to mix bits
        // This line is equivalent to 'hash = ((hash << 5) + hash) + c;' which is hash * 33 + c
        hash = ((hash << 5) + hash) + c; 
    }
    return hash;
}

void resize() {
    int old_capacity = capacity;
    capacity = 2 * capacity;
    hash_table_val *new_arr_ptr = calloc(capacity sizeof(hash_table_val));

    if(!new_arr_ptr) return; // failed to allocate

	for (int i = 0; i < old_capacity; i++) {
        if (arr_ptr[i].key && arr_ptr[i].tombstone == 0) {
            unsigned long hash_val = hash(arr_ptr[i].key);
            int index = hash_val % (capacity);

            for (int i = 0; i < old_capacity; i++) {
                int probe_index = (index + i * i) % capacity;	
                
                if (!new_arr_ptr[probe_index].key) {
                    new_arr_ptr[probe_index] = arr_ptr[i];
                    arr_ptr[i].key = NULL;
                    break;
                }
            }
        }
	
	}

    for (int i = 0; i < old_capacity; i++) {
        if (arr_ptr[i].key) {
            free(arr_ptr[i].key);
        }
    }
 
	free(arr_ptr);
    size = 0;
    for (int i = 0; i < capacity; i++) {
        if (new_arr_ptr[i].key && new_arr_ptr[i].tombstone == 0) {
            size++
        }
    }

	arr_ptr = new_arr_ptr;
}

int get(const char *key) {
	if (!key || !arr_ptr) return NULL;

	unsigned long hash_val = hash(key);    
	int index = hash_val % capacity;

	for (int i = 0; < capacity; i++) {
		int probe_index = (index + i * i) % length;	
		if (!arr_ptr[probe_index].key) {
			return -1;
		} 

        if (strcmp(arr_ptr[probe_index].key, key) == 0 && arr_ptr[probe_index].tombstone == 0) {
            return arr_ptr[probe_index].offset;
        }
	}
	return 1;
}

int insert(const char *key, long value) {
	if (!key) return NULL;
	if (value < 0) return NULL;
    if (!arr_ptr) return NULL;

    if ((double) size / capacity > 0.7) {
        resize();
    }

    unsigned long hash = hash(key);    
	int index = hash % capacity;

	hash_table_val curr_element;
	curr_element.tombstone = 0;
	curr_element.offset = value;

    curr_element.key = malloc(strlen(key) + 1)
    if (!arr_ptr[probe_index].key) return -1;
    strcpy(curr_element.key, key);

    for (int i = 0; i < capacity; i++) {
        int probe_index = (index + i * i) % capacity;	

        // if empty spot in backing array or there is a tombstone
        if (!arr_ptr[probe_index].key || arr_ptr[probe_index].tombstone == 1) {
            // need to free the existing tombstone
            if (arr_ptr[probe_index].tombstone == 1) {
                free(arr_ptr[probe_index]);
            }
            arr_ptr[probe_index] = curr_element;
            size++;
            return 0;
        }
        // updating a key if it already exists case
        if (strcmp(arr_ptr[probe_index].key, key) == 0) {
            if (arr_ptr[probe_index].tombstone == 1) {
                arr_ptr[probe_index].tombstone = 0;
                size++;
            }
            arr_ptr[probe_index].offset = value;
            return 0
        }
    }

    // table is full 
	return -1;
}

int delete(const char *key) {
	if (!key || !arr_ptr) return NULL;

	unsigned long hash = hash(key);    
	int index = hash % capacity;

	for (int i = 0; < ; i++) {
        int probe_index = (index + i * i) % capacity;
        if (!arr_ptr[probe_index]) {
            return -1;
        }
        if (strcmp(arr_ptr[probe_index].key, key)) == 0 && arr_ptr[probe_index].tombstone == 0) {
            arr_ptr[probe_index].tombstone = 1;
            size--;
            return 0;
        }
	}
	return -1; // not found in hash table
}
