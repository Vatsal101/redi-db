#include "kv.h"
#include "concurrent_hash_map.h"
#include "wal.h"

static int is_valid_key(const char *key) {
  return key != NULL && strlen(key) > 0;
}

static int is_valid_put(const char *key, const char *value) {
  return is_valid_key(key) && value != NULL && strlen(value) > 0;
}

static int append_put_record(const char *key, const char *value) {
  long offset = -1;

  if (db_append_put_record(key, value, &offset) != 0)
    return -1;
  if (concurrent_insert(get_cht(), key, offset) != 0)
    return -1;

  return 0;
}

static int append_delete_record(const char *key) {
  long offset = -1;

  if (db_append_delete_record(key, &offset) != 0)
    return -1;
  (void)offset;

  concurrent_delete(get_cht(), key);

  return 0;
}

int db_delete_table(const char *key) {
  if (!is_valid_key(key))
    return -1;

  if (concurrent_get(get_cht(), key) == -1)
    return -1;

  int result = -1;

  WalManager *wal = get_wal_manager();
  if (!wal || wal_commit_delete(wal, key) != 0)
    goto done;

  if (concurrent_get(get_cht(), key) == -1)
    result = 0;
  else
    result = append_delete_record(key);

done:
  return result;
}

// we take the pointer to the key and a pointer to the value
int db_put_table(const char *key, const char *value) {
  if (!is_valid_put(key, value))
    return -1;

  int result = -1;

  WalManager *wal = get_wal_manager();
  if (!wal || wal_commit_put(wal, key, value) != 0)
    goto done;

  result = append_put_record(key, value);

done:
  return result;
}

char *db_get_table(const char *key) {
  if (!key || strlen(key) == 0)
    return NULL;

  char header_buf[HEADER_LEN];
  record_header_t h;
  char *result = NULL;

  // get offset from hash table
  long offset = concurrent_get(get_cht(), key);
  if (offset == -1)
    goto done; // check if key exists

  ssize_t r = db_read_at(offset, header_buf, HEADER_LEN);

  if (r == 0)
    goto done; // checks if we at EOF
  if (r < 0 || r != HEADER_LEN) {
    goto done; // if not read correctly
  }

  deserialize(header_buf, &h); // deserializes header

  // if the record is a tombstone we can not go through it
  if (h.record_type == 2) {
    goto done;
  }

  // initialies keybuf with malloc since the size of key_len constantly changes
  // and we dont know this at compile time
  char *key_buf = malloc(h.key_len + 1);
  if (!key_buf) {
    goto done;
  }

  ssize_t kb = db_read_at(offset + HEADER_LEN, key_buf, h.key_len);
  if (kb < 0 || kb != h.key_len) {
    free(key_buf);
    goto done; // if not read correctly
  }
  key_buf[h.key_len] = '\0';

  // verify key matches
  if (strlen(key) != h.key_len || strncmp(key, key_buf, h.key_len) != 0) {
    free(key_buf);
    goto done; // key mismatch somethign is wrong with the index
  }

  free(key_buf);

  // read the value
  char *val_buf = malloc(h.val_len + 1);
  if (!val_buf) {
    goto done;
  }

  ssize_t vb = db_read_at(offset + HEADER_LEN + h.key_len, val_buf, h.val_len);
  if (vb < 0 || vb != h.val_len) {
    free(val_buf);
    goto done;
  }
  val_buf[h.val_len] = '\0';

  // checksum check to make sure the data is not corrupted
  uint32_t expected_crc =
      calculate_checksum(h.record_type, key, h.key_len, val_buf, h.val_len);
  if (h.crc != expected_crc) {
    free(val_buf);
    goto done;
  }

  result = val_buf; // return the value

done:
  return result;
}

int db_delete_table_internal(const char *key) {
  if (!is_valid_key(key))
    return -1;

  int result = -1;

  if (concurrent_get(get_cht(), key) == -1)
    result = 0; // recovery deletes are idempotent
  else
    result = append_delete_record(key);

  return result;
}

// we take the pointer to the key and a pointer to the value
int db_put_table_internal(const char *key, const char *value) {
  if (!is_valid_put(key, value))
    return -1;

  int result = append_put_record(key, value);
  return result;
}
