#include "wal.h"
#include "io.h"
#include "kv.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void wal_serialize(wal_header *r, char *buf) {
  if (buf == NULL || r == NULL)
    return;
  memcpy(buf, &r->record_len, sizeof(r->record_len));
  memcpy(buf + 4, &r->wal_type, sizeof(r->wal_type));
  memcpy(buf + 5, &r->txn, sizeof(r->txn));
  memcpy(buf + 9, &r->key_len, sizeof(r->key_len));
  memcpy(buf + 11, &r->val_len, sizeof(r->val_len));
  memcpy(buf + 15, &r->crc, sizeof(r->crc));
}

void wal_deserialize(char *buf, wal_header *r) {
  if (buf == NULL || r == NULL)
    return;
  memcpy(&r->record_len, buf, sizeof(r->record_len));
  memcpy(&r->wal_type, buf + 4, sizeof(r->wal_type));
  memcpy(&r->txn, buf + 5, sizeof(r->txn));
  memcpy(&r->key_len, buf + 9, sizeof(r->key_len));
  memcpy(&r->val_len, buf + 11, sizeof(r->val_len));
  memcpy(&r->crc, buf + 15, sizeof(r->crc));
}

int wal_init(WalManager *wal, FILE *fp) {
  if (!wal || !fp)
    return -1;

  wal->fp = fp;
  if (pthread_mutex_init(&wal->lock, NULL) != 0)
    return -1;
  wal->next_txn_id = 1;

  return 0;
}

static uint32_t wal_checksum(uint8_t wal_type, uint32_t txn, const char *key,
                             size_t klen, const char *value, size_t vlen) {
  uint32_t crc = 0xDEADBEEF ^ wal_type ^ txn ^ (uint32_t)klen ^ (uint32_t)vlen;

  if (key) {
    for (size_t i = 0; i < klen; i++) {
      crc ^= ((uint32_t)key[i] << (i % 24));
    }
  }
  if (value) {
    for (size_t i = 0; i < vlen; i++) {
      crc ^= ((uint32_t)value[i] << ((i + klen) % 24));
    }
  }

  return crc;
}

int wal_write_content(WalManager *wal, uint32_t txn, uint8_t wal_type,
                      const char *key, const char *value) {
  if (!wal || !wal->fp)
    return -1;

  size_t klen =
      (key && wal_type != WAL_BEGIN && wal_type != WAL_COMMIT) ? strlen(key) : 0;
  size_t vlen = (value && wal_type == WAL_PUT) ? strlen(value) : 0;

  if (klen > UINT16_MAX || vlen > UINT32_MAX)
    return -1;

  wal_header record;
  record.wal_type = wal_type;
  record.key_len = (uint16_t)klen;
  record.val_len = (uint32_t)vlen;
  record.txn = txn;
  record.record_len = WAL_HEADER_LEN + (uint32_t)klen + (uint32_t)vlen;
  record.crc = wal_checksum(wal_type, txn, key, klen, value, vlen);

  size_t total_len = WAL_HEADER_LEN + klen + vlen;
  char *full_record = malloc(total_len);
  if (!full_record)
    return -1;

  wal_serialize(&record, full_record);
  if (klen > 0)
    memcpy(full_record + WAL_HEADER_LEN, key, klen);
  if (vlen > 0)
    memcpy(full_record + WAL_HEADER_LEN + klen, value, vlen);

  int result = 0;
  if (db_append_raw_specifc(full_record, total_len, wal->fp) != 0) {
    result = -1;
  } else if (fflush(wal->fp) != 0) {
    result = -1;
  } else if (fsync(fileno(wal->fp)) != 0) {
    result = -1;
  }

  free(full_record);
  return result;
}

static int validate_wal_record(wal_header *h, char *key, char *value) {
  if (h->record_len < WAL_HEADER_LEN || h->record_len > 1024 * 1024)
    return 0;
  if (h->wal_type < WAL_BEGIN || h->wal_type > WAL_COMMIT)
    return 0;
  if (h->record_len != WAL_HEADER_LEN + h->key_len + h->val_len)
    return 0;

  return wal_checksum(h->wal_type, h->txn, key, h->key_len, value, h->val_len) ==
         h->crc;
}

static int read_wal_payload(FILE *fp, wal_header *h, char **key, char **value) {
  *key = NULL;
  *value = NULL;

  if (h->key_len > 0) {
    *key = malloc(h->key_len + 1);
    if (!*key || fread(*key, 1, h->key_len, fp) != h->key_len) {
      free(*key);
      *key = NULL;
      return -1;
    }
    (*key)[h->key_len] = '\0';
  }

  if (h->val_len > 0) {
    *value = malloc(h->val_len + 1);
    if (!*value || fread(*value, 1, h->val_len, fp) != h->val_len) {
      free(*key);
      free(*value);
      *key = NULL;
      *value = NULL;
      return -1;
    }
    (*value)[h->val_len] = '\0';
  }

  return 0;
}

int wal_crash_recovery(WalManager *wal) {
  if (!wal || !wal->fp)
    return -1;

  int result = 0;
  pthread_mutex_lock(&wal->lock);

  if (fseek(wal->fp, 0, SEEK_SET) != 0) {
    result = -1;
    goto done;
  }

  char header_buf[WAL_HEADER_LEN];

  typedef struct {
    uint32_t txn_id;
    int has_commit;
  } txn_status_t;

  txn_status_t transactions[1024];
  int txn_count = 0;
  uint32_t max_txn = 0;

  while (fread(header_buf, 1, WAL_HEADER_LEN, wal->fp) == WAL_HEADER_LEN) {
    wal_header h;
    wal_deserialize(header_buf, &h);

    if (h.record_len < WAL_HEADER_LEN || h.record_len > 1024 * 1024)
      break;

    long pos = ftell(wal->fp);
    if (pos < 0 || fseek(wal->fp, 0, SEEK_END) != 0)
      break;
    long file_end = ftell(wal->fp);
    if (file_end < 0 || fseek(wal->fp, pos, SEEK_SET) != 0)
      break;
    if (pos + h.key_len + h.val_len > file_end)
      break;

    char *key = NULL;
    char *value = NULL;
    if (read_wal_payload(wal->fp, &h, &key, &value) != 0)
      break;

    if (!validate_wal_record(&h, key, value)) {
      free(key);
      free(value);
      break;
    }

    if (h.txn > max_txn)
      max_txn = h.txn;

    if (h.wal_type == WAL_BEGIN && txn_count < 1024) {
      transactions[txn_count].txn_id = h.txn;
      transactions[txn_count].has_commit = 0;
      txn_count++;
    } else if (h.wal_type == WAL_COMMIT) {
      for (int i = 0; i < txn_count; i++) {
        if (transactions[i].txn_id == h.txn) {
          transactions[i].has_commit = 1;
          break;
        }
      }
    }

    free(key);
    free(value);
  }

  if (fseek(wal->fp, 0, SEEK_SET) != 0) {
    result = -1;
    goto done;
  }

  while (fread(header_buf, 1, WAL_HEADER_LEN, wal->fp) == WAL_HEADER_LEN) {
    wal_header h;
    wal_deserialize(header_buf, &h);

    if (h.record_len < WAL_HEADER_LEN || h.record_len > 1024 * 1024)
      break;

    long pos = ftell(wal->fp);
    if (pos < 0 || fseek(wal->fp, 0, SEEK_END) != 0)
      break;
    long file_end = ftell(wal->fp);
    if (file_end < 0 || fseek(wal->fp, pos, SEEK_SET) != 0)
      break;
    if (pos + h.key_len + h.val_len > file_end)
      break;

    char *key = NULL;
    char *value = NULL;
    if (read_wal_payload(wal->fp, &h, &key, &value) != 0)
      break;

    if (!validate_wal_record(&h, key, value)) {
      free(key);
      free(value);
      break;
    }

    int is_complete = 0;
    for (int i = 0; i < txn_count; i++) {
      if (transactions[i].txn_id == h.txn && transactions[i].has_commit) {
        is_complete = 1;
        break;
      }
    }

    if (is_complete && h.wal_type == WAL_PUT && key && value) {
      if (db_put_table_internal(key, value) != 0)
        result = -1;
    } else if (is_complete && h.wal_type == WAL_DEL && key) {
      if (db_delete_table_internal(key) != 0)
        result = -1;
    }

    free(key);
    free(value);

    if (result != 0)
      break;
  }

  if (max_txn >= wal->next_txn_id)
    wal->next_txn_id = max_txn + 1;

done:
  pthread_mutex_unlock(&wal->lock);
  return result;
}

int wal_safe_compact(WalManager *wal) {
  if (!wal || !wal->fp)
    return -1;

  int result = 0;
  pthread_mutex_lock(&wal->lock);

  FILE *main_db = get_db_file();
  if (main_db) {
    if (fflush(main_db) != 0 || fsync(fileno(main_db)) != 0) {
      result = -1;
      goto done;
    }
  }

  if (fflush(wal->fp) != 0 || fsync(fileno(wal->fp)) != 0 ||
      ftruncate(fileno(wal->fp), 0) != 0 ||
      fseek(wal->fp, 0, SEEK_SET) != 0) {
    result = -1;
  }

done:
  pthread_mutex_unlock(&wal->lock);
  return result;
}

int wal_start(WalManager *wal) {
  if (!wal)
    return -1;

  pthread_mutex_lock(&wal->lock);
  int result = wal_write_content(wal, (uint32_t)wal->next_txn_id, WAL_BEGIN, NULL, NULL);
  if (result != 0)
    pthread_mutex_unlock(&wal->lock);
  return result;
}

int wal_end(WalManager *wal) {
  if (!wal)
    return -1;

  int result = wal_write_content(wal, (uint32_t)wal->next_txn_id, WAL_COMMIT, NULL, NULL);
  if (result == 0)
    wal->next_txn_id++;

  pthread_mutex_unlock(&wal->lock);
  return result;
}

void wal_abort(WalManager *wal) {
  if (wal)
    pthread_mutex_unlock(&wal->lock);
}

int wal_put(WalManager *wal, const char *key, const char *value) {
  if (!wal || key == NULL || value == NULL || strlen(key) == 0)
    return -1;

  return wal_write_content(wal, (uint32_t)wal->next_txn_id, WAL_PUT, key, value);
}

int wal_delete(WalManager *wal, const char *key) {
  if (!wal || key == NULL || strlen(key) == 0)
    return -1;

  return wal_write_content(wal, (uint32_t)wal->next_txn_id, WAL_DEL, key, NULL);
}
