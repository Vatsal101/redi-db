#include "wal.h"

static FILE* wal_file = NULL;
int curr_txn_id = 1;

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

    record.wal_type = wal_type;
    record.key_len = (uint16_t) klen;
    record.val_len = (uint32_t) vlen;
    record.txn = txn;
    record.record_len = WAL_HEADER_LEN + (uint16_t) klen + (uint32_t) vlen;

    // calculate checksum including actual data
    record.crc = wal_type + txn + klen + vlen; // Simple checksum 

    if (key) {
        for (size_t i = 0; i < klen; i++) {
            record.crc += (uint32_t)key[i];
        }
    }
    if (value) {
        for (size_t i = 0; i < vlen; i++) {
            record.crc += (uint32_t)value[i];
        }
    }
	// char buffer we use to hold the data
	char header[WAL_HEADER_LEN];
	wal_serialize(&record, header);

	// then write to file
	if (db_append_raw_specifc(header, WAL_HEADER_LEN, wal_file) != 0) return -1;
	if (klen > 0 && db_append_raw_specifc(key, klen, wal_file) != 0) return -1;
	if (vlen > 0 && db_append_raw_specifc(value, vlen, wal_file) != 0) return -1;

    if (fflush(wal_file) != 0) return -1;	

	return 0;
}

int wal_start(){
    return wal_write_content(curr_txn_id, WAL_BEGIN, NULL, NULL);
}

int wal_end(){
    if (wal_write_content(curr_txn_id, WAL_COMMIT, NULL, NULL) != 0) {
        return -1;
    }

    curr_txn_id++;
    return 0;
}

int wal_put(const char *key, const char *value) {
    if (key == NULL || value == NULL || strlen(key) == 0) return -1;
    return wal_write_content(curr_txn_id, WAL_PUT, key, value);

}

int wal_delete(const char *key) {
    if (key == NULL || strlen(key) == 0) return -1;
    return wal_write_content(curr_txn_id, WAL_DEL, key, NULL);
}

int wal_flush() {
    if (!wal_file) return -1;
    
    // Truncate WAL file after successful recovery/compaction
    if (fflush(wal_file) != 0) return -1;
    if (ftruncate(fileno(wal_file), 0) != 0) return -1;
    if (fseek(wal_file, 0, SEEK_SET) != 0) return -1;
    
    return 0;
}

int wal_crash_recovery() {
    if (!wal_file) return -1;

    if (fseek(wal_file, 0, SEEK_SET) != 0) return -1;     
    
    char header_buf[WAL_HEADER_LEN];
    
    // define new struct that allows you to store information about the txn and whether or not it was committed succesfuly
    // I want to hold these structs in a hash table but do not know how to make another hashtable that supports keys to be integers
    typedef struct {
        uint32_t txn_id;
        int has_commit;
    } txn_status_t;
    
    txn_status_t transactions[1024]; // simple array for now
    int txn_count = 0;
    
    // scan the WAL to find the txns that are begun and committed successfully
    while (fread(header_buf, 1, WAL_HEADER_LEN, wal_file) == WAL_HEADER_LEN) {
        wal_header h;
        wal_deserialize(header_buf, &h);
        if (fseek(wal_file, h.key_len + h.val_len, SEEK_CUR) != 0) break; // moves the file pointer to the next wal_header and also checks if we reached EOF
        
        if (h.wal_type == WAL_BEGIN) { // if the log type is WAL_Begin that means we started a new change we want to document that to ensure that if there is commit then we know its good
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
    }
    
    if (fseek(wal_file, 0, SEEK_SET) != 0) return -1; // set the file pointer back to the top
    
    // implement the commits by checking if the txn has been committed
    while (fread(header_buf, 1, WAL_HEADER_LEN, wal_file) == WAL_HEADER_LEN) {
        wal_header h;
        wal_deserialize(header_buf, &h);
        
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
        
        // check if transaction is complete
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
