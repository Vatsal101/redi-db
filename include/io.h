#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>

struct WalManager;
// Record header structure (also defined in kv.h)
typedef struct {
    uint32_t record_len;  // Total length of record (header + key + value)
    uint8_t record_type;  // 1 = regular record, 2 = tombstone
    uint16_t key_len;     // Length of key
    uint32_t val_len;     // Length of value
    uint32_t crc;         // checksum = record_type + key_len + val_len
} record_header_t;

// Header size calculation
#define HEADER_LEN (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t))

// Serialization functions
void serialize(record_header_t *r, char *buf);
void deserialize(char *buf, record_header_t *r);

// Database open/close/create operations
int db_create(const char *path);
int db_open(const char *path);
void db_close(void);
FILE* get_db_file();

// Database file appending/reading operations
int db_append_raw(const void *buf, size_t len);
int db_append_raw_specifc(const void *buf, size_t len, FILE * fp);
int db_append_put_record(const char *key, const char *value, long *offset_out);
int db_append_delete_record(const char *key, long *offset_out);
int db_read_at(long offset, void *buf, size_t len);

// File pointer manipulation and information
int db_rewind(void);
long get_curr_offset(void);

// Misallencous (index table, compaction, wal manager, db_file)
int fill_offset_table(void);
int db_compact(const char *path);
struct WalManager* get_wal_manager(void);

#include "concurrent_hash_map.h"
ConcurrentHashTable* get_cht(void);


uint32_t calculate_checksum(uint8_t record_type, const char *key, uint16_t key_len, const char *value, uint32_t val_len);

#endif // IO_H
