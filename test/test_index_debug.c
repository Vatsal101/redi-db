#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kv.h"
#include "io.h"
#include "index.h"

void test_hash_table_resize_debug() {
    printf("Testing hash table resize functionality with debug...\n");
    
    db_close();
    remove("test_resize_debug.db");
    
    assert(db_create("test_resize_debug.db") == 0);
    
    // Insert records one by one and monitor
    int num_records = 25; // This should trigger resize
    
    for (int i = 0; i < num_records; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "resize_key_%03d", i);
        snprintf(value, sizeof(value), "resize_value_%03d", i);
        
        printf("\n=== Inserting record %d: key='%s' ===\n", i, key);
        int result = db_put_table(key, value);
        if (result != 0) {
            printf("Failed to insert record %d\n", i);
            assert(0);
        }
        printf("Successfully inserted record %d\n", i);
    }
    
    printf("\n=== Verifying all records ===\n");
    
    // Verify all records are still accessible
    for (int i = 0; i < num_records; i++) {
        char key[32], expected_value[64];
        snprintf(key, sizeof(key), "resize_key_%03d", i);
        snprintf(expected_value, sizeof(expected_value), "resize_value_%03d", i);
        
        printf("Verifying key '%s'\n", key);
        char *val = db_get_table(key);
        if (!val) {
            printf("ERROR: Failed to retrieve key '%s'\n", key);
            assert(0);
        }
        if (strcmp(val, expected_value) != 0) {
            printf("ERROR: Value mismatch for key '%s'. Expected '%s', got '%s'\n", 
                   key, expected_value, val);
            assert(0);
        }
        free(val);
        printf("Verified key '%s' successfully\n", key);
    }
    
    printf("All records verified successfully\n");
    db_close();
    printf("✓ Hash table resize debug test passed\n");
}

int main() {
    test_hash_table_resize_debug();
    return 0;
}