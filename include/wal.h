#ifndef WAL_H
#define WAL_H

#include "io.h"

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
    uint32_t crc32;         // Checksum
} wal_header;


int wal_put();
int wal_delete();
int wal_flush();
int wal_write_record();

extern int next_txn_id;

#define WAL_HEADER_LEN (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t))
#endif

