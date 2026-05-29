#include "io.h"
#include "concurrent_hash_map.h"
#include "wal.h"
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

// p_db_file should be binary file
static FILE *p_db_file = NULL;
static WalManager wal;
static int wal_initialized = 0;
static ConcurrentHashTable curr_cht;

ConcurrentHashTable *get_cht(void) { return &curr_cht; }

WalManager *get_wal_manager(void) { return wal_initialized ? &wal : NULL; }

FILE *get_db_file() { return p_db_file; }
// We serialize data to ensure that byte order is consistent
// we get the raw data within the struct
void serialize(record_header_t *r, char *buf) {
  if (buf == NULL || r == NULL)
    return;
  memcpy(buf, &r->record_len, sizeof(r->record_len));
  memcpy(buf + 4, &r->record_type, sizeof(r->record_type));
  memcpy(buf + 5, &r->key_len, sizeof(r->key_len));
  memcpy(buf + 7, &r->val_len, sizeof(r->val_len));
  memcpy(buf + 11, &r->crc, sizeof(r->crc));
}

// we need to deserialize data so we take the raw byte
void deserialize(char *buf, record_header_t *r) {
  if (buf == NULL || r == NULL)
    return;
  memcpy(&r->record_len, buf, sizeof(r->record_len));
  memcpy(&r->record_type, buf + 4, sizeof(r->record_type));
  memcpy(&r->key_len, buf + 5, sizeof(r->key_len));
  memcpy(&r->val_len, buf + 7, sizeof(r->val_len));
  memcpy(&r->crc, buf + 11, sizeof(r->crc));
}

void db_close() {
  if (p_db_file) {
    fclose(p_db_file);
    p_db_file = NULL;
  }
  if (wal_initialized) {
    if (wal.fp)
      fclose(wal.fp);
    pthread_mutex_destroy(&wal.lock);
    pthread_cond_destroy(&wal.durable_cv);
    wal.fp = NULL;
    wal_initialized = 0;
  }
  // Clean up hash table when closing database
  cleanup_concurrent_hash_table(&curr_cht);
}

int db_create(const char *path) {
  if (!path)
    return -1;
  if (p_db_file || wal_initialized)
    db_close();

  char path_copy[strlen(path) + 5]; // create new string which has a little more
                                    // space than the path len
  strcpy(path_copy, path);          // copy the path into this path_copy
  char wal_path[] = ".wal";

  p_db_file = fopen(path, "w+b");
  FILE *wal_fp = fopen(
      strcat(path_copy, wal_path),
      "w+b"); // concat path with the wal ending and create file with that name

  if (!p_db_file || !wal_fp) {
    if (p_db_file)
      fclose(p_db_file);
    if (wal_fp)
      fclose(wal_fp);
    p_db_file = NULL;
    return -1;
  }

  // Initialize hash table when creating a new database
  cleanup_concurrent_hash_table(&curr_cht); // Clean up any existing hash table first

  if (init_concurrent_hash_table(&curr_cht) != 0) {
    fclose(p_db_file);
    fclose(wal_fp);
    p_db_file = NULL;
    return -1;
  }

  if (wal_init(&wal, wal_fp) != 0) {
    fclose(p_db_file);
    fclose(wal_fp);
    p_db_file = NULL;
    return -1;
  }
  wal_initialized = 1;
  return 0;
}

// Uses fwrite to append to binary file of a serialzed general buffer
// Checks if the length of what was written equals to the inputted length
// need to use fwrite to write to binary file
// it will read from inputted data with count elements of size (size) and then
// write this into the file fwrite(data, size, count, fptr) data = name of the
// array to be written, size = size of each element, count = number of elements
// to be written, ftpr - file pointer returns the number of items written
// sucessfully
int db_append_raw(const void *buf, size_t len) {
  if (!p_db_file || !buf)
    return -1;
  // ensure the writes will be going to the end of the file
  if (fseek(p_db_file, 0, SEEK_END) != 0) {
    return -1;
  }
  // append the raw data to the file and check if it was written
  //
  size_t written = fwrite(buf, len, 1, p_db_file);
  if (written != 1) {
    return -1;
  }
  // flush to make sure its written to OS buffers
  if (fflush(p_db_file) != 0) {
    return -1;
  }

  if (fsync(fileno(p_db_file)) != 0) {
    return -1;
  }
  return 0;
}

int db_append_raw_specifc(const void *buf, size_t len, FILE *fp) {
  if (!fp || !buf)
    return -1;
  // ensure the writes will be going to the end of the file
  if (fseek(fp, 0, SEEK_END) != 0) {
    return -1;
  }
  // append the raw data to the file and check if it was written
  //
  size_t written = fwrite(buf, len, 1, fp);
  if (written != 1) {
    return -1;
  }
  // flush to make sure its written to OS buffers
  if (fflush(fp) != 0) {
    return -1;
  }

  return 0;
}
// since the buffer is not a const it means it can be modified that means that
// buf is going to be an abstract way to store the data we get from our db when
// we are done storing it. This works by first moving the file pointer to the
// specific location we want which is decided by the offset number Then we read
// from this location up to the length of the data which we know and then write
// this info to a buffer which we return fread(source_addr, size, count,
// destination_addr) RETURNS: how many bytes were read which should be 11 since
// that is how many we are expecting might change to be ready for partial reads
int db_read_at(long offset, void *buf, size_t len) {
  if (!p_db_file || !buf)
    return -1;

  int seek_result = fseek(p_db_file, offset, SEEK_SET);
  if (seek_result != 0) {
    return -1;
  }

  size_t bytes_read = fread(buf, 1, len, p_db_file);
  if (bytes_read != len) {
    if (feof(p_db_file)) {
      return 0; // EOF reached
    }
    if (ferror(p_db_file)) {
      clearerr(p_db_file);
      return -1;
    }
  }

  // returns how many bytes were successfully read by fread
  return (int)bytes_read;
}

int db_rewind() {
  if (!p_db_file)
    return -1;

  if (fseek(p_db_file, 0, SEEK_SET) != 0) {
    return -1;
  }
  return 0;
}

int fill_offset_table() {
  if (!p_db_file)
    return -1;

  if (db_rewind() != 0)
    return -1;

  char header_buf[HEADER_LEN];
  record_header_t h;

  while (1) {
    // Get current file position BEFORE reading
    long current_pos = ftell(p_db_file);
    if (current_pos == -1)
      break;

    // Try to read header at current position
    size_t bytes_read = fread(header_buf, 1, HEADER_LEN, p_db_file);
    if (bytes_read == 0)
      break; // EOF
    if (bytes_read != HEADER_LEN)
      break; // Incomplete read

    deserialize(header_buf, &h); // deserializes header

    // Read the key
    char *key_buf = malloc(h.key_len + 1);
    if (!key_buf) {
      return -1;
    }

    bytes_read = fread(key_buf, 1, h.key_len, p_db_file);
    if (bytes_read != h.key_len) {
      free(key_buf);
      return -1; // if not read correctly
    }
    key_buf[h.key_len] = '\0';

    // Always insert/update the key with the latest offset
    // This handles both regular records and tombstones
    if (h.record_type == 1) {
      // Regular record - insert or update
      if (concurrent_insert(&curr_cht, key_buf, current_pos) != 0) {
        free(key_buf);
        return -1;
      }
    } else if (h.record_type == 2) {
      // Tombstone - remove from hash table
      concurrent_delete(&curr_cht, key_buf);
    }

    free(key_buf);

    // Skip the value to move to next record
    if (fseek(p_db_file, h.val_len, SEEK_CUR) != 0) {
      break; // Error seeking
    }
  }
  return 0;
}

int db_open(const char *path) {
  if (!path)
    return -1;
  if (p_db_file || wal_initialized)
    db_close();

  p_db_file = fopen(path, "r+b");
  if (!p_db_file)
    return -1;

  char wal_path[strlen(path) + 5];
  strcpy(wal_path, path);
  strcat(wal_path, ".wal");
  FILE *wal_fp = fopen(wal_path, "r+b");
  if (!wal_fp) {
    // Create WAL file if it doesn't exist
    wal_fp = fopen(wal_path, "w+b");
    if (!wal_fp) {
      fclose(p_db_file);
      p_db_file = NULL;
      return -1;
    }
  }

  // Initialize hash table when opening an existing database
  

  cleanup_concurrent_hash_table(&curr_cht); // Clean up any existing hash table first

  if (init_concurrent_hash_table(&curr_cht) != 0) {
    fclose(p_db_file);
    fclose(wal_fp);
    p_db_file = NULL;
    return -1;
  }

  

  // Rebuild the hash table from the existing data
  if (fill_offset_table() != 0) {
    cleanup_concurrent_hash_table(&curr_cht);
    fclose(p_db_file);
    fclose(wal_fp);
    p_db_file = NULL;
    return -1;
  }

  // Initialize WAL
  if (wal_init(&wal, wal_fp) != 0) {
    fclose(p_db_file);
    fclose(wal_fp);
    p_db_file = NULL;
    return -1;
  }
  wal_initialized = 1;

  // Perform WAL recovery
  if (wal_crash_recovery(&wal) != 0) {
    db_close();
    return -1;
  }

  if (db_compact(path) != 0) {
    db_close();
    return -1;
  }

  if (wal_safe_compact(&wal) != 0) {
    return -1;
  }

  return 0;
}

long get_curr_offset() {
  long current_offset = ftell(p_db_file);

  if (current_offset == -1L) {
    return -1; // somethign is wrong with the offset
  } else {
    return current_offset; // this is the current offset of the file pointer is
                           // at
  }
}

/*

creates a new temp file
then iterates through the hashmap and adds these values in the hashmap to the
new temp file the values in the hashamp are the latest put records and thus we
skip deleted keys replaces files atomicaly
*/
int db_compact(const char *path) {
  char filename_template[] = "/tmp/my_temp_fileXXXXXX";
  int fd;
  FILE *f;

  // Create and open the unique temporary file
  fd = mkstemp(filename_template);

  if (fd == -1)
    return -1;

  f = fdopen(fd, "w+b");

  if (!f) {
    close(fd);
    unlink(filename_template);
    return -1;
  }

  // FILE *f = fopen("temp.db", "w+b");
  // if (!f) return -1;

  char header_buf[HEADER_LEN];
  record_header_t h;

  for (int bi = 0; bi < curr_cht.capacity; bi++) {
    Bucket *bucket = &curr_cht.bucket_ptr[bi];

    pthread_rwlock_wrlock(&bucket->lock);

    HashTable *map = bucket->map;
    hash_table_val *arr_ptr = map->arr_ptr;

    for (int i = 0; i < map->capacity; i++) {
      if (arr_ptr[i].key == NULL || arr_ptr[i].tombstone)
        continue;

      long offset = arr_ptr[i].offset;

      if (fseek(p_db_file, offset, SEEK_SET) != 0)
        continue;

      size_t bytes_read = fread(header_buf, 1, HEADER_LEN, p_db_file);
      if (bytes_read == 0)
        continue;
      if (bytes_read != HEADER_LEN)
        continue;

      deserialize(header_buf, &h);

      int klen = h.key_len;
      int vlen = h.val_len;

      char *key_buf = malloc(klen + 1);
      if (!key_buf) {
        pthread_rwlock_unlock(&bucket->lock);
        fclose(f);
        unlink(filename_template);
        return -1;
      }

      ssize_t kb = db_read_at(offset + HEADER_LEN, key_buf, klen);
      if (kb < 0 || kb != klen) {
        free(key_buf);
        continue;
      }
      key_buf[klen] = '\0';

      char *val_buf = malloc(vlen + 1);
      if (!val_buf) {
        free(key_buf);
        pthread_rwlock_unlock(&bucket->lock);
        fclose(f);
        unlink(filename_template);
        return -1;
      }

      ssize_t vb = db_read_at(offset + HEADER_LEN + klen, val_buf, vlen);
      if (vb < 0 || vb != vlen) {
        free(val_buf);
        free(key_buf);
        continue;
      }
      val_buf[vlen] = '\0';

      if (h.crc != calculate_checksum(h.record_type, key_buf, h.key_len,
                                      val_buf, h.val_len)) {
        free(val_buf);
        free(key_buf);
        pthread_rwlock_unlock(&bucket->lock);
        fclose(f);
        unlink(filename_template);
        return -1;
      }

      long current_offset = ftell(f);
      if (current_offset == -1) {
        free(val_buf);
        free(key_buf);
        pthread_rwlock_unlock(&bucket->lock);
        fclose(f);
        unlink(filename_template);
        return -1;
      }

      if (db_append_raw_specifc(header_buf, HEADER_LEN, f) != 0 ||
          db_append_raw_specifc(key_buf, klen, f) != 0 ||
          db_append_raw_specifc(val_buf, vlen, f) != 0) {
        free(val_buf);
        free(key_buf);
        pthread_rwlock_unlock(&bucket->lock);
        fclose(f);
        unlink(filename_template);
        return -1;
      }

      arr_ptr[i].offset = current_offset;

      free(val_buf);
      free(key_buf);
    }

    pthread_rwlock_unlock(&bucket->lock);
  }

  // close the stream which closes the fd
  fclose(f);

  if (p_db_file) {
    fclose(p_db_file);
    p_db_file = NULL;
  }

  if (rename(filename_template, path) == -1) {
    unlink(filename_template); // Clean up temporary file
    return -1;
  }

  // Reopen the compacted file
  p_db_file = fopen(path, "r+b");
  if (!p_db_file)
    return -1;

  return 0;
}

// simple checksum
uint32_t calculate_checksum(uint8_t record_type, const char *key,
                            uint16_t key_len, const char *value,
                            uint32_t val_len) {
  uint32_t checksum = record_type + key_len + val_len;

  // key content
  for (uint16_t i = 0; i < key_len; i++) {
    checksum += (uint32_t)key[i];
  }

  // value content unless if tombstone
  if (value && val_len > 0) {
    for (uint32_t i = 0; i < val_len; i++) {
      checksum += (uint32_t)value[i];
    }
  }

  return checksum;
}
