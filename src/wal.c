#include "wal.h"
#include <unistd.h>

static FILE* wal_file = NULL;
int curr_txn_id = 0;

void wal_serialize(wal_header *r, char *buf) {
	if (buf == NULL || r == NULL) return;
	memcpy(buf, &r->record_len, sizeof(r->record_len));
	memcpy(buf + 4, &r->wal_type, sizeof(r->wal_type));
	memcpy(buf + 5, &r->txn, sizeof(r->txn));
	memcpy(buf + 9, &r->key_len, sizeof(r->key_len));
	memcpy(buf + 11, &r->val_len, sizeof(r->val_len));
	memcpy(buf + 15, &r->crc, sizeof(r->crc));
}

// we need to deserialize data so we take the raw byte
void wal_deserialize(char *buf, wal_header *r) {
	if (buf == NULL || r == NULL) return;
	memcpy(&r->record_len, buf, sizeof(r->record_len));
	memcpy(&r->wal_type, buf + 4,sizeof(r->wal_type));
	memcpy(&r->txn, buf + 5, sizeof(r->txn)); 
	memcpy(&r->key_len, buf + 9, sizeof(r->key_len));
	memcpy(&r->val_len, buf + 11, sizeof(r->val_len));
	memcpy(&r->crc, buf + 15, sizeof(r->crc));
}

int wal_init() {
   if (get_wal_file() != NULL) {
        wal_file = get_wal_file();
        return 0;
   }
   return -1;
}

int wal_write_content(uint32_t txn, uint8_t wal_type, const char *key, const char *value) {
    if (!wal_file) return -1;

    wal_header record;
    size_t klen = (key && wal_type != WAL_BEGIN && wal_type != WAL_COMMIT) ? strlen(key) : 0;
    size_t vlen = (value && wal_type == WAL_PUT) ? strlen(value) : 0;

    // Validate sizes to prevent overflow
    if (klen > UINT16_MAX || vlen > UINT32_MAX) return -1;
    
    record.wal_type = wal_type;
    record.key_len = (uint16_t)klen;
    record.val_len = (uint32_t)vlen;
    record.txn = txn;
    record.record_len = WAL_HEADER_LEN + klen + vlen;
    
    // Stronger checksum including magic number
    record.crc = 0xDEADBEEF ^ wal_type ^ txn ^ klen ^ vlen;
    if (key) {
        for (size_t i = 0; i < klen; i++) {
            record.crc ^= ((uint32_t)key[i] << (i % 24));
        }
    }
    if (value) {
        for (size_t i = 0; i < vlen; i++) {
            record.crc ^= ((uint32_t)value[i] << ((i + klen) % 24));
        }
    }

    size_t total_len = WAL_HEADER_LEN + klen + vlen; 
    char *full_record = malloc(total_len); // this is the full record including WAL_HEADER, Key, value
    if (!full_record) return -1;
    
    wal_serialize(&record, full_record);
    if (key) memcpy(full_record + WAL_HEADER_LEN, key, klen);
    if (value) memcpy(full_record + WAL_HEADER_LEN + klen, value, vlen);
    
    // Single atomic write
    if (db_append_raw_specifc(full_record, total_len, wal_file) != 0) {
        free(full_record);
        return -1;
    }
    
    free(full_record);
    
    if (fflush(wal_file) != 0) return -1;
    if (fsync(fileno(wal_file)) != 0) return -1;  // Actually force to disk
    
    return 0;
}

static int validate_wal_record(wal_header *h, char *key, char *value) {
    if (h->record_len < WAL_HEADER_LEN || h->record_len > 1024 * 1024) return 0;
    if (h->wal_type < WAL_BEGIN || h->wal_type > WAL_COMMIT) return 0;
    if (h->record_len != WAL_HEADER_LEN + h->key_len + h->val_len) return 0;
    
    uint32_t expected_crc = 0xDEADBEEF ^ h->wal_type ^ h->txn ^ h->key_len ^ h->val_len;
    if (key) {
        for (uint16_t i = 0; i < h->key_len; i++) {
            expected_crc ^= ((uint32_t)key[i] << (i % 24));
        }
    }
    if (value) {
        for (uint32_t i = 0; i < h->val_len; i++) {
            expected_crc ^= ((uint32_t)value[i] << ((i + h->key_len) % 24));
        }
    }
    
    return expected_crc == h->crc;
}

int wal_crash_recovery() {
    if (!wal_file) return -1;
    // sets the file pointer to the top
    if (fseek(wal_file, 0, SEEK_SET) != 0) return -1;
    
    char header_buf[WAL_HEADER_LEN];
    
    // define new struct that allows you to store information about the txn and whether or not it was committed succesfuly
    // I want to hold these structs in a hash table but do not know how to make another hashtable that supports keys to be integers
    typedef struct {
        uint32_t txn_id;
        int has_commit;
    } txn_status_t;
    
    txn_status_t transactions[1024];
    int txn_count = 0;
    
    // FIRST PASS: Validate all records and find commits
    while (fread(header_buf, 1, WAL_HEADER_LEN, wal_file) == WAL_HEADER_LEN) {
        wal_header h;
        wal_deserialize(header_buf, &h);
        
        // PARTIAL WRITE DETECTION
        if (h.record_len < WAL_HEADER_LEN || h.record_len > 1024 * 1024) {
            break; // Invalid record, stop processing
        }
        
        // Check if we can read the full record
        long pos = ftell(wal_file);
        if (fseek(wal_file, 0, SEEK_END) != 0) break;
        long file_end = ftell(wal_file);
        if (fseek(wal_file, pos, SEEK_SET) != 0) break;
        
        if (pos + h.key_len + h.val_len > file_end) {
            break; // Partial record at end of file
        }
        
        // Read and validate key/value data
        char *key = NULL, *value = NULL;
        if (h.key_len > 0) {
            key = malloc(h.key_len + 1);
            if (!key || fread(key, 1, h.key_len, wal_file) != h.key_len) {
                free(key);
                break;
            }
            key[h.key_len] = '\0';
        }
        
        if (h.val_len > 0) {
            value = malloc(h.val_len + 1);
            if (!value || fread(value, 1, h.val_len, wal_file) != h.val_len) {
                free(key);
                free(value);
                break;
            }
            value[h.val_len] = '\0';
        }
        
        if (!validate_wal_record(&h, key, value)) {
            free(key);
            free(value);
            break; // Corrupted record
        }
        
        // Track transaction states
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
    
    // SECOND PASS: Replay only validated, committed transactions
    if (fseek(wal_file, 0, SEEK_SET) != 0) return -1;
    
    // implement the commits by checking if the txn has been committed
    while (fread(header_buf, 1, WAL_HEADER_LEN, wal_file) == WAL_HEADER_LEN) {
        wal_header h;
        wal_deserialize(header_buf, &h);
        
        // Re-validate (defensive programming)
        if (h.record_len < WAL_HEADER_LEN) break;
        
        char *key = NULL, *value = NULL;
        if (h.key_len > 0) {
            key = malloc(h.key_len + 1);
            if (!key || fread(key, 1, h.key_len, wal_file) != h.key_len) {
                free(key);
                break;
            }
            key[h.key_len] = '\0';
        }
        
        if (h.val_len > 0) {
            value = malloc(h.val_len + 1);
            if (!value || fread(value, 1, h.val_len, wal_file) != h.val_len) {
                free(key);
                free(value);
                break;
            }
            value[h.val_len] = '\0';
        }
        
        if (!validate_wal_record(&h, key, value)) {
            free(key);
            free(value);
            break;
        }
        
        // Check if transaction is committed
        int is_complete = 0;
        for (int i = 0; i < txn_count; i++) {
            if (transactions[i].txn_id == h.txn && transactions[i].has_commit) {
                is_complete = 1;
                break;
            }
        }
        
        // replay operations from complete transactions ( logic to know when to replay transactions)
        if (is_complete && (h.wal_type == WAL_PUT || h.wal_type == WAL_DEL)) {
            if (h.wal_type == WAL_PUT && key && value) {
                db_put_table_internal(key, value);
            } else if (h.wal_type == WAL_DEL && key) {
                db_delete_table_internal(key);
            }
        }
        
        free(key);
        free(value);
    }
    
    return 0;
}

// SAFE COMPACTION: Only clear WAL after ensuring consistency
int wal_safe_compact() {
    if (!wal_file) return -1;
    
    // Force main database to disk first
    FILE *main_db = get_db_file();
    if (main_db) {
        if (fflush(main_db) != 0) return -1;
        if (fsync(fileno(main_db)) != 0) return -1;
    }
    
    // Now safe to clear WAL
    if (fflush(wal_file) != 0) return -1;
    if (fsync(fileno(wal_file)) != 0) return -1;
    if (ftruncate(fileno(wal_file), 0) != 0) return -1;
    if (fseek(wal_file, 0, SEEK_SET) != 0) return -1;
    
    return 0;
}

int wal_start() {
    return wal_write_content(curr_txn_id, WAL_BEGIN, NULL, NULL);
}

int wal_end() {
    int result = wal_write_content(curr_txn_id, WAL_COMMIT, NULL, NULL);
    if (result == 0) {
        curr_txn_id++; 
    }
    return result;
}

int wal_put(const char *key, const char *value) {
    if (key == NULL || value == NULL || strlen(key) == 0) return -1;
    return wal_write_content(curr_txn_id, WAL_PUT, key, value);

}

int wal_delete(const char *key) {
    if (key == NULL || strlen(key) == 0) return -1;
    return wal_write_content(curr_txn_id, WAL_DEL, key, NULL);
}
