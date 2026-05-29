#ifndef WAL_H
#define WAL_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
  WAL_BEGIN = 1,
  WAL_PUT = 2,
  WAL_DEL = 3,
  WAL_COMMIT = 4
} wal_type;

typedef struct {
  uint32_t record_len; // total record length
  uint8_t wal_type;    // wal_type
  uint32_t txn;        // txn id
  uint16_t key_len;    // Length of key
  uint32_t val_len;    // Length of value
  uint32_t crc;        // Checksum
} wal_header;

typedef struct WalManager {
  FILE *fp;
  pthread_mutex_t lock;
  pthread_cond_t
      durable_cv; // lets writers sleep until their commits have begun durable

  uint64_t
      durable_lsn; // highest byte offset known that has already been synced
  uint64_t written_lsn; // highest byte offset written to WAL file buffer
  uint64_t next_txn_id;

  int sync_in_progress; // tells waiting writers that another thread is already
                        // doing an fsync
} WalManager;

void wal_serialize(wal_header *r, char *buf);
void wal_deserialize(char *buf, wal_header *r);
int wal_init(WalManager *wal, FILE *fp);
int wal_write_content(WalManager *wal, uint32_t txn, uint8_t wal_type,
                      const char *key, const char *value);
int wal_start(WalManager *wal);
int wal_end(WalManager *wal);
void wal_abort(WalManager *wal);
int wal_put(WalManager *wal, const char *key, const char *value);
int wal_delete(WalManager *wal, const char *key);
int wal_safe_compact(WalManager *wal);
int wal_crash_recovery(WalManager *wal);

#define WAL_HEADER_LEN                                                         \
  (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t) +  \
   sizeof(uint32_t) + sizeof(uint32_t))

#endif
