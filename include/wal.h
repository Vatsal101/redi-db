#ifndef WAL_H
#define WAL_H

#include "kv.h"

typedef enum {
    WAL_BEGIN = 1,
    WAL_PUT   = 2,
    WAL_DEL   = 3,
    WAL_COMMIT = 4
} wal_type;

typedef struct {
    uint32_t record_len;       // total record length
    uint8_t  wal_type;      // wal_type
    uint32_t txn;           // txn id 
    uint16_t key_len;       // Length of key 
    uint32_t val_len;       // Length of value 
    uint32_t crc;         // Checksum
} wal_header;

void wal_serialize(wal_header *r, char *buf);
void wal_deserialize(char *buf, wal_header *r);
int wal_init();
int wal_write_content(uint32_t txn, uint8_t wal_type, const char *key, const char *value);
int wal_start();
int wal_end();
int wal_put(const char *key, const char *value);
int wal_delete(const char *key);

extern int curr_txn_id;

#define WAL_HEADER_LEN (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t))

#endif

