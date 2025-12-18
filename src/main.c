#include <stdio.h>
#include <stdlib.h>
#include "kv.h"
#include "io.h"

void test_get(const char *key, const char *expected) {
    printf("Getting key '%s'...\n", key);
    char *v = db_get(key);
    if (v) { 
        printf("  %s -> %s", key, v);
        if (expected) {
            printf(" (expected: %s)\n", expected);
        } else {
            printf(" (expected: not found - ERROR!)\n");
        }
        free(v); 
    } else {
        if (expected) {
            printf("  %s not found (expected: %s - ERROR!)\n", key, expected);
        } else {
            printf("  %s not found (as expected)\n", key);
        }
    }
}

int main(void) {
    printf("=== SimpleDB Tombstone Test ===\n\n");
    
    printf("Opening database...\n");
    if (db_open("tombstone_test.db") != 0) { 
        perror("Failed to open database"); 
        return 1; 
    }
    printf("Database opened successfully\n\n");

    // Test 1: Basic put/get
    printf("--- Test 1: Basic Operations ---\n");
    printf("Putting key 'foo' = 'hello'...\n");
    if (db_put("foo", "hello") != 0) { 
        printf("put 'foo' failed\n"); 
        return 1;
    }
    
    printf("Putting key 'bar' = 'world'...\n");
    if (db_put("bar", "world") != 0) { 
        printf("put 'bar' failed\n"); 
        return 1;
    }
    
    test_get("foo", "hello");
    test_get("bar", "world");
    printf("\n");

    // Test 2: Delete (tombstone) operations
    printf("--- Test 2: Delete Operations ---\n");
    printf("Deleting key 'foo'...\n");
    if (db_delete("foo") != 0) {
        printf("delete 'foo' failed\n");
        return 1;
    }
    
    test_get("foo", NULL);  // Should not be found
    test_get("bar", "world");  // Should still exist
    printf("\n");

    // Test 3: Put after delete (resurrection)
    printf("--- Test 3: Put After Delete ---\n");
    printf("Putting key 'foo' = 'resurrected' (after delete)...\n");
    if (db_put("foo", "resurrected") != 0) {
        printf("put 'foo' after delete failed\n");
        return 1;
    }
    
    test_get("foo", "resurrected");  // Should find new value
    test_get("bar", "world");  // Should still exist
    printf("\n");

    // Test 4: Multiple updates and deletes
    printf("--- Test 4: Multiple Operations ---\n");
    printf("Updating 'bar' = 'updated'...\n");
    if (db_put("bar", "updated") != 0) {
        printf("update 'bar' failed\n");
        return 1;
    }
    
    printf("Updating 'bar' = 'updated_again'...\n");
    if (db_put("bar", "updated_again") != 0) {
        printf("second update 'bar' failed\n");
        return 1;
    }
    
    test_get("bar", "updated_again");
    
    printf("Deleting 'bar'...\n");
    if (db_delete("bar") != 0) {
        printf("delete 'bar' failed\n");
        return 1;
    }
    
    test_get("bar", NULL);  // Should not be found
    test_get("foo", "resurrected");  // Should still exist
    printf("\n");

    // Test 5: Delete non-existent key
    printf("--- Test 5: Delete Non-existent Key ---\n");
    printf("Deleting non-existent key 'nonexistent'...\n");
    if (db_delete("nonexistent") != 0) {
        printf("delete 'nonexistent' failed\n");
        return 1;
    }
    
    test_get("nonexistent", NULL);  // Should not be found
    printf("\n");

    // Test 6: Complex scenario
    printf("--- Test 6: Complex Scenario ---\n");
    printf("Adding key 'test' = 'value1'...\n");
    db_put("test", "value1");
    test_get("test", "value1");
    
    printf("Deleting 'test'...\n");
    db_delete("test");
    test_get("test", NULL);
    
    printf("Adding 'test' = 'value2'...\n");
    db_put("test", "value2");
    test_get("test", "value2");
    
    printf("Deleting 'test' again...\n");
    db_delete("test");
    test_get("test", NULL);
    
    printf("Adding 'test' = 'final_value'...\n");
    db_put("test", "final_value");
    test_get("test", "final_value");
    printf("\n");

    // Final state check
    printf("--- Final State ---\n");
    test_get("foo", "resurrected");
    test_get("bar", NULL);
    test_get("test", "final_value");
    test_get("nonexistent", NULL);

    printf("\nClosing database...\n");
    db_close();
    printf("Test completed successfully!\n");
    return 0;
}