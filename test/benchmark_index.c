#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "kv.h"
#include "io.h"
#include "index.h"

void benchmark_performance() {
    printf("Benchmarking index vs linear scan performance...\n");
    
    db_close();
    remove("benchmark.db");
    
    assert(db_create("benchmark.db") == 0);
    
    int num_records = 5000;
    int num_lookups = 100;
    
    printf("Inserting %d records...\n", num_records);
    
    // Insert records
    clock_t start = clock();
    for (int i = 0; i < num_records; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "benchmark_key_%05d", i);
        snprintf(value, sizeof(value), "benchmark_value_%05d", i);
        
        assert(db_put_table(key, value) == 0);
    }
    clock_t end = clock();
    
    double insert_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Insert time: %.3f seconds (%.1f records/sec)\n", 
           insert_time, num_records / insert_time);
    
    // Benchmark index-based lookups
    printf("\nTesting %d index-based lookups...\n", num_lookups);
    start = clock();
    
    for (int i = 0; i < num_lookups; i++) {
        char key[32];
        // Look for keys distributed throughout the dataset
        snprintf(key, sizeof(key), "benchmark_key_%05d", 
                 (i * num_records) / num_lookups);
        
        char *val = db_get_table(key);
        assert(val != NULL);
        free(val);
    }
    
    end = clock();
    double index_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Index lookup time: %.3f seconds (%.1f lookups/sec)\n", 
           index_time, num_lookups / index_time);
    
    // Benchmark linear scan lookups
    printf("\nTesting %d linear scan lookups...\n", num_lookups);
    start = clock();
    
    for (int i = 0; i < num_lookups; i++) {
        char key[32];
        snprintf(key, sizeof(key), "benchmark_key_%05d", 
                 (i * num_records) / num_lookups);
        
        char *val = db_get(key);
        assert(val != NULL);
        free(val);
    }
    
    end = clock();
    double linear_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Linear scan time: %.3f seconds (%.1f lookups/sec)\n", 
           linear_time, num_lookups / linear_time);
    
    printf("\n📊 Performance Results:\n");
    printf("Index-based lookup: %.1fx faster than linear scan\n", 
           linear_time / index_time);
    
    db_close();
    remove("benchmark.db");
}

int main() {
    benchmark_performance();
    return 0;
}