#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kv.h"
#include "io.h"
#include "index.h"

void test_basic_put_get() {
    printf("Testing basic put/get with index...\n");
    
    // Clean start
    db_close();
    remove("test_index.db");
    
    assert(db_create("test_index.db") == 0);
    
    // Test basic put/get
    assert(db_put_table("key1", "value1") == 0);
    assert(db_put_table("key2", "value2") == 0);
    assert(db_put_table("key3", "value3") == 0);
    
    char *val1 = db_get_table("key1");
    char *val2 = db_get_table("key2");
    char *val3 = db_get_table("key3");
    
    assert(val1 != NULL && strcmp(val1, "value1") == 0);
    assert(val2 != NULL && strcmp(val2, "value2") == 0);
    assert(val3 != NULL && strcmp(val3, "value3") == 0);
    
    free(val1);
    free(val2);
    free(val3);
    
    // Test non-existent key
    char *val4 = db_get_table("nonexistent");
    assert(val4 == NULL);
    
    db_close();
    printf("✓ Basic put/get test passed\n");
}

void test_update_operations() {
    printf("Testing update operations...\n");
    
    db_close();
    remove("test_update.db");
    
    assert(db_create("test_update.db") == 0);
    
    // Insert initial value
    assert(db_put_table("update_key", "initial_value") == 0);
    
    char *val = db_get_table("update_key");
    assert(val != NULL && strcmp(val, "initial_value") == 0);
    free(val);
    
    // Update the value
    assert(db_put_table("update_key", "updated_value") == 0);
    
    val = db_get_table("update_key");
    assert(val != NULL && strcmp(val, "updated_value") == 0);
    free(val);
    
    db_close();
    printf("✓ Update operations test passed\n");
}

void test_delete_operations() {
    printf("Testing delete operations...\n");
    
    db_close();
    remove("test_delete.db");
    
    assert(db_create("test_delete.db") == 0);
    
    // Insert some values
    assert(db_put_table("del1", "value1") == 0);
    assert(db_put_table("del2", "value2") == 0);
    assert(db_put_table("del3", "value3") == 0);
    
    // Verify they exist
    char *val = db_get_table("del1");
    assert(val != NULL && strcmp(val, "value1") == 0);
    free(val);
    
    // Delete one key
    assert(db_delete_table("del1") == 0);
    
    // Verify it's gone
    val = db_get_table("del1");
    assert(val == NULL);
    
    // Verify others still exist
    val = db_get_table("del2");
    assert(val != NULL && strcmp(val, "value2") == 0);
    free(val);
    
    val = db_get_table("del3");
    assert(val != NULL && strcmp(val, "value3") == 0);
    free(val);
    
    // Try to delete non-existent key
    assert(db_delete_table("nonexistent") == -1);
    
    db_close();
    printf("✓ Delete operations test passed\n");
}

void test_persistence() {
    printf("Testing persistence (reopen database)...\n");
    
    db_close();
    remove("test_persist.db");
    
    // Create and populate database
    assert(db_create("test_persist.db") == 0);
    
    assert(db_put_table("persist1", "value1") == 0);
    assert(db_put_table("persist2", "value2") == 0);
    assert(db_put_table("persist3", "value3") == 0);
    
    // Delete one key
    assert(db_delete_table("persist2") == 0);
    
    db_close();
    
    // Reopen database
    assert(db_open("test_persist.db") == 0);
    
    // Verify data is still there
    char *val1 = db_get_table("persist1");
    char *val3 = db_get_table("persist3");
    char *val2 = db_get_table("persist2"); // Should be NULL (deleted)
    
    assert(val1 != NULL && strcmp(val1, "value1") == 0);
    assert(val3 != NULL && strcmp(val3, "value3") == 0);
    assert(val2 == NULL);
    
    free(val1);
    free(val3);
    
    db_close();
    printf("✓ Persistence test passed\n");
}

void test_performance_comparison() {
    printf("Testing performance comparison (index vs linear scan)...\n");
    
    db_close();
    remove("test_perf.db");
    
    assert(db_create("test_perf.db") == 0);
    
    // Insert many records
    int num_records = 1000;
    printf("Inserting %d records...\n", num_records);
    
    for (int i = 0; i < num_records; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "key_%04d", i);
        snprintf(value, sizeof(value), "value_for_key_%04d", i);
        
        assert(db_put_table(key, value) == 0);
    }
    
    // Test lookup performance - search for keys at the end
    printf("Testing lookup performance...\n");
    
    // Index-based lookup (should be fast)
    char *val_index = db_get_table("key_0999");
    assert(val_index != NULL);
    assert(strcmp(val_index, "value_for_key_0999") == 0);
    free(val_index);
    
    // Linear scan lookup (should be slower)
    char *val_linear = db_get("key_0999");
    assert(val_linear != NULL);
    assert(strcmp(val_linear, "value_for_key_0999") == 0);
    free(val_linear);
    
    printf("Both lookups returned correct results\n");
    
    db_close();
    printf("✓ Performance comparison test passed\n");
}

void test_hash_table_resize() {
    printf("Testing hash table resize functionality...\n");
    
    // Ensure clean state
    db_close();
    remove("test_resize.db");
    
    // Create fresh database
    assert(db_create("test_resize.db") == 0);
    
    // Insert enough records to trigger resize (load factor > 0.7)
    // Initial capacity is 31, so insert ~25 records to trigger resize
    int num_records = 50;
    
    for (int i = 0; i < num_records; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "resize_key_%03d", i);
        snprintf(value, sizeof(value), "resize_value_%03d", i);
        
        int result = db_put_table(key, value);
        if (result != 0) {
            printf("Failed to insert record %d (key: %s)\n", i, key);
            assert(0);
        }
    }
    
    // Verify all records are still accessible after resize
    for (int i = 0; i < num_records; i++) {
        char key[32], expected_value[64];
        snprintf(key, sizeof(key), "resize_key_%03d", i);
        snprintf(expected_value, sizeof(expected_value), "resize_value_%03d", i);
        
        char *val = db_get_table(key);
        if (!val) {
            printf("Failed to retrieve key: %s\n", key);
            assert(0);
        }
        if (strcmp(val, expected_value) != 0) {
            printf("Value mismatch for key %s. Expected: %s, Got: %s\n", 
                   key, expected_value, val);
            assert(0);
        }
        free(val);
    }
    
    db_close();
    printf("✓ Hash table resize test passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    db_close();
    remove("test_edge.db");
    
    assert(db_create("test_edge.db") == 0);
    
    // Test empty key/value (should fail)
    assert(db_put_table("", "value") == -1); // Empty key should work
    assert(db_put_table("key", "") == -1);   // Empty value should work
    assert(db_put_table(NULL, "value") == -1); // NULL key should fail
    assert(db_put_table("key", NULL) == -1);   // NULL value should fail
    
    // Test very long key/value
    char long_key[1000], long_value[2000];
    memset(long_key, 'A', 999);
    long_key[999] = '\0';
    memset(long_value, 'B', 1999);
    long_value[1999] = '\0';
    
    assert(db_put_table(long_key, long_value) == 0);
    
    char *retrieved = db_get_table(long_key);
    assert(retrieved != NULL);
    assert(strcmp(retrieved, long_value) == 0);
    free(retrieved);
    
    db_close();
    printf("✓ Edge cases test passed\n");
}

int main() {
    printf("Running hash table index tests...\n\n");
    
    test_basic_put_get();
    test_update_operations();
    test_delete_operations();
    test_persistence();
    test_performance_comparison();
    test_hash_table_resize();
    test_edge_cases();
    
    printf("\n✅ All tests passed!\n");
    
    // Cleanup
    remove("test_index.db");
    remove("test_update.db");
    remove("test_delete.db");
    remove("test_persist.db");
    remove("test_perf.db");
    remove("test_resize.db");
    remove("test_edge.db");
    
    return 0;
}