#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index.h"

hash_table_val *arr_ptr;
int starting_elements = 31;
int size = 0;
int capacity = 31;

// initializes the hash table 
int init_hash_table(void) {
    arr_ptr = calloc(starting_elements, sizeof(hash_table_val));
    if (!arr_ptr) return -1;
    size = 0;
    capacity = starting_elements;
    return 0;
}
// safely destroys the hash table and freeing all the strings in the hash table
void cleanup_hash_table(void) {
    if (arr_ptr) {
        for (int i = 0; i < capacity; i++) {
	// free all the strings in the arr_ptr[i] struct
            if (arr_ptr[i].key) {
                free(arr_ptr[i].key);
                arr_ptr[i].key = NULL;
            }
        }
	// frees the pointer to the hashtable 
        free(arr_ptr);
        arr_ptr = NULL;
    }
    size = 0;
    capacity = starting_elements;
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
	// first we are iterating through all the old elements in the old hash table
    for (int i = 0; i < old_capacity; i++) {
	// in an element exists at the index and its not a tombstone we want to insert it to new hash table
        if (old_arr_ptr[i].key && old_arr_ptr[i].tombstone == 0) {
            // Find a spot in the new table
            unsigned long hash_val = hash(old_arr_ptr[i].key);
            int index = hash_val % capacity;

	    // we are inserting the old value into the new hash table and need to probe and do the same insert functionality	
            for (int j = 0; j < capacity; j++) {
                int probe_index = (index + j * j) % capacity;

        	// if the spot is empty we just set the new informaiton equal to the old 
                if (!arr_ptr[probe_index].key) {
                    arr_ptr[probe_index].key = old_arr_ptr[i].key; // sets the pointer to the key to the new arr_ptr
                    arr_ptr[probe_index].offset = old_arr_ptr[i].offset;
                    arr_ptr[probe_index].tombstone = 0;
		    free(old_arr_ptr[i].key); // need to free this memory and then mark the pointer as null
                    old_arr_ptr[i].key = NULL; // Prevent double-free
                    size++;
                    break;
                }
            }
        }
    }
    
    // Clean up the old array free keys that weren't transferred
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

    // the max number of probes we want to do is the capacity of the array
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
                // Different key, replace it
            if (strcmp(arr_ptr[probe_index].key, key) != 0) {
                free(arr_ptr[probe_index].key);
                arr_ptr[probe_index].key = malloc(strlen(key) + 1);
                if (!arr_ptr[probe_index].key) return -1;
                strcpy(arr_ptr[probe_index].key, key);
            }
            // Reuse the existing key memory if it's the same key
            arr_ptr[probe_index].offset = value;
	    arr_ptr[probe_index].tombstone = 0;
	    size++;
            return 0;
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
            return -1; // this means that no possible value so you cant make a tombstone
        }
        if (strcmp(arr_ptr[probe_index].key, key) == 0 && arr_ptr[probe_index].tombstone == 0) {
            arr_ptr[probe_index].tombstone = 1;
            size--;
            return 0;
        }
    }
    
    return -1;
}
