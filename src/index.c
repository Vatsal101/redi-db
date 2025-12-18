#include <stdio.h>
#include <stdlib.h>
#include "index.h"

int *arr_ptr;
int starting_elements = 7;
int size = 0;

arr_ptr = malloc(starting_elements * sizeof(hash_table_val))

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


int insert(const char *key, const int value) {
    if (!key) return NULL;
    if (value < 0) return NULL;

    int length = sizeof(arr_ptr) / sizeof(arr_ptr[0])

    unsigned long hash = hash(key);    
    int index = hash % length;

    hash_table_val curr_element;
    curr_element.tombstone = 0;
    curr_element.offset = value;


}