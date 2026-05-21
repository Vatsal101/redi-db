#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "index.h"

// hash_table_val *arr_ptr;
// int starting_elements = 31;
// int size = 0;
// int capacity = 31;

// initializes the hash table 
int init_hash_table(HashTable *ht) {
    if (!ht) return -1;
    ht->starting_elements = 31;
    ht->arr_ptr = calloc(ht->starting_elements, sizeof(hash_table_val));
    if (!ht->arr_ptr) return -1;
    ht->size = 0;
    ht->capacity = ht->starting_elements;
    return 0;
}
// safely destroys the hash table and freeing all the strings in the hash table
void cleanup_hash_table(HashTable *ht) {
    if (!ht) return -1;
    if (ht->arr_ptr) {
        for (int i = 0; i < ht->capacity; i++) {
	// free all the strings in the arr_ptr[i] struct
            if (ht->arr_ptr[i].key) {
                free(ht->arr_ptr[i].key);
                ht->arr_ptr[i].key = NULL;
            }
        }
	// frees the pointer to the hashtable 
        free(ht->arr_ptr);
        ht->arr_ptr = NULL;
    }
    ht->size = 0;
    ht->capacity = ht->starting_elements;
}

// gets the hashes for the string
unsigned long hash(const char *str) {
    if (!str) return 0; // Handle NULL case gracefully
    
    unsigned long hash = 5381; 
    int c;
    while ((c = *str++)) { 
        hash = ((hash << 5) + hash) + c; 
    }
    return hash;
}

// resize method for my hash table
void resize(HashTable *ht) {
    if (!ht) return -1;

    int old_capacity = ht->capacity;
    hash_table_val *old_arr_ptr = ht->arr_ptr;

    ht->capacity = 2 * ht->capacity;
    ht->arr_ptr = calloc(ht->capacity, sizeof(hash_table_val));
    
    if (!ht->arr_ptr) {
        // Restore old state if allocation fails
        ht->capacity = old_capacity;
        ht->arr_ptr = old_arr_ptr;
        return;
    }
    
    // Reset size 
    ht->size = 0;
    
    // Rehash all existing elements
	// first we are iterating through all the old elements in the old hash table
    for (int i = 0; i < old_capacity; i++) {
	// in an element exists at the index and its not a tombstone we want to insert it to new hash table
        if (old_arr_ptr[i].key && old_arr_ptr[i].tombstone == 0) {
            // Find a spot in the new table
            unsigned long hash_val = hash(old_arr_ptr[i].key);
            int index = hash_val % ht->capacity;

	    // we are inserting the old value into the new hash table and need to probe and do the same insert functionality	
            for (int j = 0; j < ht->capacity; j++) {
                int probe_index = (index + j * j) % ht->capacity;

        	// if the spot is empty we just set the new informaiton equal to the old 
                if (!ht->arr_ptr[probe_index].key) {
                    ht->arr_ptr[probe_index].key = old_arr_ptr[i].key; // sets the pointer to the key to the new arr_ptr
                    ht->arr_ptr[probe_index].offset = old_arr_ptr[i].offset;
                    ht->arr_ptr[probe_index].tombstone = 0;
                    old_arr_ptr[i].key = NULL; // had a memory leak here where i was freeing the string which was being pointed to instead of setting the old pointer to null 
                    ht->size++;
                    break;
                }
            }
        }
    }
    
    // Clean up the old array free keys that weren't transferred
    for (int i = 0; i < old_capacity; i++) {
        if (old_arr_ptr[i].key != NULL) {
            free(old_arr_ptr[i].key);
        }
    }
    
    free(old_arr_ptr);
}

int get(HashTable *ht, const char *key) {
    if (!ht || !key || !ht->arr_ptr) return -1;

    unsigned long hash_val = hash(key);    
    int index = hash_val % ht->capacity;

    // the max number of probes we want to do is the capacity of the array
    for (int i = 0; i < ht->capacity; i++) {
        int probe_index = (index + i * i) % ht->capacity;
        if (!ht->arr_ptr[probe_index].key) {
            return -1; // Empty slot found, key doesn't exist
        } 

        if (strcmp(ht->arr_ptr[probe_index].key, key) == 0 && ht->arr_ptr[probe_index].tombstone == 0) {
            return ht->arr_ptr[probe_index].offset;
        }
    }

    return -1;
}

int insert(HashTable *ht, const char *key, long value) {

    if (!ht || !key || value < 0 || !ht->arr_ptr) return -1;

    if ((double) ht->size / ht->capacity > 0.7) {
        resize(ht);
    }

    unsigned long hash_val = hash(key);    
    int index = hash_val % ht->capacity;

    for (int i = 0; i < ht->capacity; i++) {
        int probe_index = (index + i * i) % ht->capacity;	

        // if empty spot in backing array
        if (!ht->arr_ptr[probe_index].key) {
            ht->arr_ptr[probe_index].key = malloc(strlen(key) + 1);
            if (!ht->arr_ptr[probe_index].key) return -1;
            strcpy(ht->arr_ptr[probe_index].key, key);
            ht->arr_ptr[probe_index].offset = value;
            ht->arr_ptr[probe_index].tombstone = 0;
            ht->size++;
            return 0;
        }
        
        // if there is a tombstone at this position
        if (ht->arr_ptr[probe_index].tombstone == 1) {
                // Different key, replace it
            if (strcmp(ht->arr_ptr[probe_index].key, key) != 0) {
                free(ht->arr_ptr[probe_index].key);
                ht->arr_ptr[probe_index].key = malloc(strlen(key) + 1);
                if (!ht->arr_ptr[probe_index].key) return -1;
                strcpy(ht->arr_ptr[probe_index].key, key);
            }
            // Reuse the existing key memory if it's the same key
            ht->arr_ptr[probe_index].offset = value;
	    ht->arr_ptr[probe_index].tombstone = 0;
	    ht->size++;
        return 0;
        }
        
        // updating a key if it already exists case (not tombstone)
        if (strcmp(ht->arr_ptr[probe_index].key, key) == 0) {
            ht->arr_ptr[probe_index].offset = value;
            return 0;
        }
    }

    // table is full 

    return -1;
}

int delete(HashTable *ht, const char *key) {
    if (!ht || !key || !ht->arr_ptr) return -1;

    int capacity = ht->capacity;
    hash_table_val *arr_ptr = ht->arr_ptr;
    
    unsigned long hash_val = hash(key);    
    int index = hash_val % capacity;

    for (int i = 0; i < capacity; i++) {
        int probe_index = (index + i * i) % capacity;
        if (!arr_ptr[probe_index].key) {
            return -1; // this means that no possible value so you cant make a tombstone
        }
        if (strcmp(arr_ptr[probe_index].key, key) == 0 && arr_ptr[probe_index].tombstone == 0) {
            arr_ptr[probe_index].tombstone = 1;
            ht->size--;
            return 0;
        }
    }
    
    return -1;
}
