#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
// Record header structure (also defined in kv.h)
typedef struct {
    uint32_t record_len;  // Total length of record (header + key + value)
    uint8_t record_type;  // 1 = regular record, 2 = tombstone
    uint16_t key_len;     // Length of key
    uint32_t val_len;     // Length of value
} record_header_t;

// Header size calculation
#define HEADER_LEN (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t))

// Serialization functions
void serialize(record_header_t *r, char *buf);
void deserialize(char *buf, record_header_t *r);

// Database file operations
int db_create(const char *path);
int db_open(const char *path);
void db_close(void);
int db_append_raw(const void *buf, size_t len);
int db_read_at(long offset, void *buf, size_t len);
int db_rewind(void);
long get_curr_offset(void);
int fill_offset_table(void);

#endif // IO_H
