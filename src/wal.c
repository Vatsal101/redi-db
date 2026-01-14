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
    if (!wal_type) return -1;

    wal_header record;

    size_t klen = (key && wal_type != WAL_BEGIN && wal_type != WAL_COMMIT) ? strlen(key) : 0;
    size_t vlen = (value && wal_type == WAL_PUT) ? strlen(value) : 0;

    record.wal_type = wal_type;
    record.key_len = (uint16_t) klen;
    record.val_len = (uint32_t) vlen;
    record.txn = txn;
    record.record_len = WAL_HEADER_LEN + (uint16_t) klen + (uint32_t) vlen;

    // calculate checksum including actual data
    record.crc = type + txn + klen + vlen; // Simple checksum 

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
	serialize(&record, header);

	// then write to file
	if (db_append_raw_specifc(header, WAL_HEADER_LEN, wal_file) != 0) return -1;
	if (klen > 0 && db_append_raw_specifc(key, klen, wal_file) != 0) return -1;
	if (vlen > 0 && db_append_raw_specifc(value, vlen, wal_file) != 0) return -1;

    if (fflush(wal_file) != 0) return -1;	

	return 0;
}

int wal_start(){
    wal_write_content(curr_txn_id, WAL_BEGIN, NULL, NULL)
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
    return wal_write_content(curr_txn_id, WAL_PUT, key, value)

}

int wal_delete(const char *key) {
    if (key == NULL || strlen(key) == 0) return -1;
    return wal_write_content(curr_txn_id, WAL_DEL, key, NULL)
}



