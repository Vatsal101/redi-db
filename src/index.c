#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index.h"

hash_table_val *arr_ptr;
int starting_elements = 31;
int size = 0;
int capacity = 31;

int init_hash_table(void) {
    arr_ptr = calloc(starting_elements, sizeof(hash_table_val));
    if (!arr_ptr) return -1;
    size = 0;
    capacity = starting_elements;
    return 0;
}

void cleanup_hash_table(void) {
    if (arr_ptr) {
        for (int i = 0; i < capacity; i++) {
            if (arr_ptr[i].key) {
                free(arr_ptr[i].key);
                arr_ptr[i].key = NULL;
            }
        }
        free(arr_ptr);
        arr_ptr = NULL;
    }
    size = 0;
    capacity = starting_elements;
}

unsigned long hash(const char *str) {
    if (!str) return 0; // Handle NULL case gracefully
    
    unsigned long hash = 5381; 
    int c;
    while ((c = *str++)) { 
        hash = ((hash << 5) + hash) + c; 
    }
    return hash;
}

void resize(void) {
    int old_capacity = capacity;
    hash_table_val *old_arr_ptr = arr_ptr;
    
    // Double the capacity
    capacity = 2 * capacity;
    arr_ptr = calloc(capacity, sizeof(hash_table_val));
    
    if (!arr_ptr) {
        // Restore old state if allocation fails
        capacity = old_capacity;
        arr_ptr = old_arr_ptr;
        return;
    }
    
    // Reset size for recounting
    size = 0;
    
    // Rehash all existing elements
    for (int i = 0; i < old_capacity; i++) {
        if (old_arr_ptr[i].key && old_arr_ptr[i].tombstone == 0) {
            // Find a spot in the new table
            unsigned long hash_val = hash(old_arr_ptr[i].key);
            int index = hash_val % capacity;
            
            for (int j = 0; j < capacity; j++) {
                int probe_index = (index + j * j) % capacity;
                
                if (!arr_ptr[probe_index].key) {
                    // Transfer the key (don't copy)
                    arr_ptr[probe_index].key = old_arr_ptr[i].key;
                    arr_ptr[probe_index].offset = old_arr_ptr[i].offset;
                    arr_ptr[probe_index].tombstone = 0;
                    old_arr_ptr[i].key = NULL; // Prevent double-free
                    size++;
                    break;
                }
            }
        }
    }
    
    // Clean up the old array - only free keys that weren't transferred
    for (int i = 0; i < old_capacity; i++) {
        if (old_arr_ptr[i].key) {
            free(old_arr_ptr[i].key);
        }
    }
    free(old_arr_ptr);
}

int get(const char *key) {
    if (!key || !arr_ptr) return -1;

    unsigned long hash_val = hash(key);    
    int index = hash_val % capacity;

    for (int i = 0; i < capacity; i++) {
        int probe_index = (index + i * i) % capacity;	
        if (!arr_ptr[probe_index].key) {
            return -1; // Empty slot found, key doesn't exist
        } 

        if (strcmp(arr_ptr[probe_index].key, key) == 0 && arr_ptr[probe_index].tombstone == 0) {
            return arr_ptr[probe_index].offset;
        }
    }
    return -1;
}

int insert(const char *key, long value) {
    if (!key || value < 0 || !arr_ptr) return -1;

    if ((double) size / capacity > 0.7) {
        resize();
    }

    unsigned long hash_val = hash(key);    
    int index = hash_val % capacity;

    for (int i = 0; i < capacity; i++) {
        int probe_index = (index + i * i) % capacity;	

        // if empty spot in backing array
        if (!arr_ptr[probe_index].key) {
            arr_ptr[probe_index].key = malloc(strlen(key) + 1);
            if (!arr_ptr[probe_index].key) return -1;
            strcpy(arr_ptr[probe_index].key, key);
            arr_ptr[probe_index].offset = value;
            arr_ptr[probe_index].tombstone = 0;
            size++;
            return 0;
        }
        
        // if there is a tombstone at this position
        if (arr_ptr[probe_index].tombstone == 1) {
            // Reuse the existing key memory if it's the same key
            if (strcmp(arr_ptr[probe_index].key, key) == 0) {
                arr_ptr[probe_index].offset = value;
                arr_ptr[probe_index].tombstone = 0;
                size++;
                return 0;
            } else {
                // Different key, replace it
                free(arr_ptr[probe_index].key);
                arr_ptr[probe_index].key = malloc(strlen(key) + 1);
                if (!arr_ptr[probe_index].key) return -1;
                strcpy(arr_ptr[probe_index].key, key);
                arr_ptr[probe_index].offset = value;
                arr_ptr[probe_index].tombstone = 0;
                size++;
                return 0;
            }
        }
        
        // updating a key if it already exists case (not tombstone)
        if (strcmp(arr_ptr[probe_index].key, key) == 0) {
            arr_ptr[probe_index].offset = value;
            return 0;
        }
    }

    // table is full 
    return -1;
}

int delete(const char *key) {
    if (!key || !arr_ptr) return -1;

    unsigned long hash_val = hash(key);    
    int index = hash_val % capacity;

    for (int i = 0; i < capacity; i++) {
        int probe_index = (index + i * i) % capacity;
        if (!arr_ptr[probe_index].key) {
            return -1; // this means that no input so you cant make a tombstone
        }
        if (strcmp(arr_ptr[probe_index].key, key) == 0 && arr_ptr[probe_index].tombstone == 0) {
            arr_ptr[probe_index].tombstone = 1;
            size--;
            return 0;
        }
    }
    
    return -1;
}
