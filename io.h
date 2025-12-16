#ifndef IO_H
#define IO_H

#include <stdint.h>  // For uint32_t, uint8_t, uint16_t
#include <stddef.h>  // For size_t
#include <stdio.h>   // For FILE*

typedef struct {
	uint32_t record_len; // bytes after this length field
	uint8_t record_type; // 1= put, 2 = tombstone
	uint16_t key_len; // length of the key
	uint32_t val_len;  // length of the value
} record_header_t;

int db_open(const char *path);
int db_create(const char *path);
int db_append_raw(const void *buf, size_t len);
int db_read_at(long offset, void *buf, size_t len);
int db_rewind();
void db_close();
#endif
