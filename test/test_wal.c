#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include "wal.h"
#include "io.h"
#include "kv.h"

// Test helper functions
void cleanup_test_files() {
    unlink("test.db");
    unlink("test.db.wal");
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

void print_test_result(const char *test_name, int passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
    if (!passed) {
        printf("  ❌ Test failed!\n");
    }
}

// Test 1: Basic WAL Operations
int test_basic_wal_operations() {
    printf("\n=== Test 1: Basic WAL Operations ===\n");
    
    cleanup_test_files();
    
    // Create database
    if (db_create("test.db") != 0) {
        printf("Failed to create database\n");
        return 0;
    }
    
    // Initialize WAL
    if (wal_init() != 0) {
        printf("Failed to initialize WAL\n");
        return 0;
    }
    
    // Test basic put operation with WAL
    if (db_put_table("key1", "value1") != 0) {
        printf("Failed to put key1\n");
        return 0;
    }
    
    // Verify WAL file was created and has content
    int wal_exists = file_exists("test.db.wal");
    long wal_size = get_file_size("test.db.wal");
    
    printf("  WAL file exists: %s\n", wal_exists ? "Yes" : "No");
    printf("  WAL file size: %ld bytes\n", wal_size);
    
    // Verify data can be retrieved
    char *retrieved = db_get_table("key1");
    int data_correct = (retrieved && strcmp(retrieved, "value1") == 0);
    if (retrieved) free(retrieved);
    
    db_close();
    
    return wal_exists && wal_size > 0 && data_correct;
}

// Test 2: Transaction Boundaries
int test_transaction_boundaries() {
    printf("\n=== Test 2: Transaction Boundaries ===\n");
    
    cleanup_test_files();
    
    if (db_create("test.db") != 0) return 0;
    if (wal_init() != 0) return 0;
    
    printf("  Initial transaction ID: %d\n", curr_txn_id);
    
    // First transaction
    if (db_put_table("txn1_key", "txn1_value") != 0) return 0;
    printf("  After first transaction: %d\n", curr_txn_id);
    
    // Second transaction  
    if (db_put_table("txn2_key", "txn2_value") != 0) return 0;
    printf("  After second transaction: %d\n", curr_txn_id);
    
    // Verify transaction IDs incremented correctly
    int txn_ids_correct = (curr_txn_id == 3); // Should be 3 after 2 transactions
    
    db_close();
    return txn_ids_correct;
}

// Test 3: WAL Recovery
int test_wal_recovery() {
    printf("\n=== Test 3: WAL Recovery ===\n");
    
    cleanup_test_files();
    
    // Create database and add data
    if (db_create("test.db") != 0) return 0;
    if (wal_init() != 0) return 0;
    
    if (db_put_table("recover_key1", "recover_value1") != 0) return 0;
    if (db_put_table("recover_key2", "recover_value2") != 0) return 0;
    if (db_delete_table("recover_key1") != 0) return 0; // Delete first key
    
    printf("  Added data and closed database\n");
    db_close();
    
    // Simulate restart - open database again
    if (db_open("test.db") != 0) {
        printf("Failed to reopen database\n");
        return 0;
    }
    
    // Verify WAL recovery worked
    char *val1 = db_get_table("recover_key1"); // Should be NULL (deleted)
    char *val2 = db_get_table("recover_key2"); // Should exist
    
    int recovery_correct = (val1 == NULL && val2 != NULL && strcmp(val2, "recover_value2") == 0);
    
    printf("  Key1 after recovery (should be deleted): %s\n", val1 ? val1 : "NULL");
    printf("  Key2 after recovery: %s\n", val2 ? val2 : "NULL");
    
    if (val1) free(val1);
    if (val2) free(val2);
    
    db_close();
    return recovery_correct;
}

// Test 4: Partial Write Detection
int test_partial_write_detection() {
    printf("\n=== Test 4: Partial Write Detection ===\n");
    
    cleanup_test_files();
    
    if (db_create("test.db") != 0) return 0;
    if (wal_init() != 0) return 0;
    
    // Add some valid data
    if (db_put_table("valid_key", "valid_value") != 0) return 0;
    
    db_close();
    
    // Manually corrupt WAL file by truncating it
    FILE *wal_file = fopen("test.db.wal", "r+b");
    if (!wal_file) return 0;
    
    // Get current size and truncate to simulate partial write
    fseek(wal_file, 0, SEEK_END);
    long size = ftell(wal_file);
    printf("  Original WAL size: %ld bytes\n", size);
    
    // Truncate to create partial record
    if (ftruncate(fileno(wal_file), size - 10) != 0) {
        fclose(wal_file);
        return 0;
    }
    
    fclose(wal_file);
    
    printf("  Truncated WAL to simulate partial write\n");
    
    // Try to open database - should handle partial write gracefully
    int open_result = db_open("test.db");
    
    if (open_result == 0) {
        printf("  Database opened successfully despite corrupted WAL\n");
        
        // Try to verify some data still works
        char *val = db_get_table("valid_key");
        printf("  Retrieved value: %s\n", val ? val : "NULL");
        if (val) free(val);
        
        db_close();
        return 1;
    } else {
        printf("  Database failed to open with corrupted WAL\n");
        return 0;
    }
}

// Test 5: WAL Compaction
int test_wal_compaction() {
    printf("\n=== Test 5: WAL Compaction ===\n");
    
    cleanup_test_files();
    
    if (db_create("test.db") != 0) return 0;
    if (wal_init() != 0) return 0;
    
    // Add multiple operations to build up WAL
    for (int i = 0; i < 5; i++) {
        char key[20], value[20];
        sprintf(key, "compact_key%d", i);
        sprintf(value, "compact_value%d", i);
        if (db_put_table(key, value) != 0) return 0;
    }
    
    long wal_size_before = get_file_size("test.db.wal");
    printf("  WAL size before compaction: %ld bytes\n", wal_size_before);
    
    // Perform safe compaction
    if (wal_safe_compact() != 0) {
        printf("  WAL compaction failed\n");
        db_close();
        return 0;
    }
    
    long wal_size_after = get_file_size("test.db.wal");
    printf("  WAL size after compaction: %ld bytes\n", wal_size_after);
    
    // Verify data is still accessible
    char *val = db_get_table("compact_key2");
    int data_intact = (val && strcmp(val, "compact_value2") == 0);
    if (val) free(val);
    
    db_close();
    
    // WAL should be much smaller after compaction
    return (wal_size_after < wal_size_before) && data_intact;
}

// Test 6: Stress Test - Multiple Operations
int test_stress_operations() {
    printf("\n=== Test 6: Stress Test - Multiple Operations ===\n");
    
    cleanup_test_files();
    
    if (db_create("test.db") != 0) return 0;
    if (wal_init() != 0) return 0;
    
    int operations = 100;
    printf("  Performing %d operations...\n", operations);
    
    // Mix of puts and deletes
    for (int i = 0; i < operations; i++) {
        char key[20], value[30];
        sprintf(key, "stress_key%d", i);
        sprintf(value, "stress_value_%d_data", i);
        
        if (db_put_table(key, value) != 0) {
            printf("  Failed at put operation %d\n", i);
            db_close();
            return 0;
        }
        
        // Delete every 3rd key
        if (i % 3 == 0 && i > 0) {
            char delete_key[20];
            sprintf(delete_key, "stress_key%d", i-1);
            if (db_delete_table(delete_key) != 0) {
                printf("  Failed at delete operation %d\n", i);
                db_close();
                return 0;
            }
        }
    }
    
    printf("  Completed %d operations\n", operations);
    printf("  Final transaction ID: %d\n", curr_txn_id);
    printf("  WAL file size: %ld bytes\n", get_file_size("test.db.wal"));
    
    // Verify some data - use key49 which survives (key50 gets deleted when i=51)
    char *val = db_get_table("stress_key49");
    int verification = (val && strstr(val, "stress_value_49") != NULL);
    if (val) free(val);
    
    db_close();
    return verification;
}

// Main test runner
int main() {
    printf("🔧 WAL Implementation Test Suite\n");
    printf("================================\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Run all tests
    struct {
        const char *name;
        int (*test_func)();
    } tests[] = {
        {"Basic WAL Operations", test_basic_wal_operations},
        {"Transaction Boundaries", test_transaction_boundaries}, 
        {"WAL Recovery", test_wal_recovery},
        {"Partial Write Detection", test_partial_write_detection},
        {"WAL Compaction", test_wal_compaction},
        {"Stress Test Operations", test_stress_operations}
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        total_tests++;
        int result = tests[i].test_func();
        if (result) passed_tests++;
        print_test_result(tests[i].name, result);
    }
    
    printf("\n📊 Test Results Summary\n");
    printf("=======================\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    printf("Success rate: %.1f%%\n", (float)passed_tests / total_tests * 100);
    
    // Cleanup
    cleanup_test_files();
    
    if (passed_tests == total_tests) {
        printf("\n✅ All tests passed! Your WAL implementation looks solid.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Check the issues above.\n");
        return 1;
    }
}