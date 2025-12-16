#include "kv.h"
#include <string.h>
#include <stdlib.h>  // For malloc/free
#include <sys/types.h> // For ssize_t

// We serialize data to ensure that byte order is consistent
// we get the raw data within the struct
void serialize(record_header_t *r, char *buf) {
	if (buf == NULL || r == NULL) return;
	memcpy(buf, &r->record_len, sizeof(r->record_len));
	memcpy(buf + 4, &r->record_type, sizeof(r->record_type));
	memcpy(buf + 5, &r->key_len, sizeof(r->key_len));
	memcpy(buf + 7, &r->val_len, sizeof(r->val_len));
}

// we need to deserialize data so we take the raw byte
void deserialize(char *buf, record_header_t *r) {
	if (buf == NULL || r == NULL) return;
	memcpy(&r->record_len, buf, sizeof(r->record_len));
	memcpy(&r->record_type, buf + 4,sizeof(r->record_type));
	memcpy(&r->key_len, buf + 5, sizeof(r->key_len));
	memcpy(&r->val_len, buf + 7, sizeof(r->val_len));
}


// we take the pointer to the key and a pointer to the value
int db_put(const char *key, const char *value) {
	if (key == NULL || value == NULL) return -1;

	record_header_t record;
	size_t klen = strlen(key);
	size_t vlen = strlen(value);

	record.record_len = HEADER_LEN + (uint16_t) klen + (uint32_t) vlen;
	record.record_type = 1;
	record.key_len = (uint16_t) strlen(key);
	record.val_len = (uint32_t) strlen(value);
	
	// char buffer we use to hold the data
	char header[HEADER_LEN];
	
	// uses the size of the keys, value, records to seralize the data to a buffer 	
	serialize(&record, header);
	
	// uses the general buffer as way to put the data into the db
	
	if (db_append_raw(header, HEADER_LEN) != 0) return -1;
	if (db_append_raw(key, klen) != 0) return -1;
	if (db_append_raw(value, vlen) != 0) return -1;

	return 0;
}


char *db_get(const char *key) {
	if (!key) return NULL;
	uint16_t keylen = strlen(key);
	db_rewind();

	char header_buf[HEADER_LEN];
	record_header_t h;
	long offset = 0;

	char *found = NULL;

	while(1) {
		ssize_t r = db_read_at(offset, header_buf, HEADER_LEN);

		if (r == 0) break; // checks if we at EOF
		if (r < 0 || r != HEADER_LEN) return NULL; // if not read correctly

		deserialize(header_buf, &h); // deserializes header

		// initialies keybuf with malloc since the size of key_len constantly changes and we dont know this at compile time		
		char *key_buf = malloc(h.key_len + 1);	
		if (!key_buf) {return NULL;}

		ssize_t kb = db_read_at(offset + HEADER_LEN, key_buf, h.key_len);
		if (kb < 0 || kb != h.key_len) {
			free(key_buf);
			return NULL; // if not read correctly
		}
		key_buf[h.key_len] = '\0';
		
		// same logic as key buf
		char *val_buf = malloc(h.val_len + 1);	
		if (!val_buf) {
			free(key_buf); 
			return NULL;
		}	
		
		ssize_t vb = db_read_at(offset + HEADER_LEN + h.key_len, val_buf, h.val_len);
		if (vb < 0 || vb != h.val_len) {
			free(val_buf);
			free(key_buf);
			return NULL; // if not read correctly
		}
		val_buf[h.val_len] = '\0';

		if (h.key_len == keylen && strncmp(key, key_buf, keylen) == 0) {
			free(found); // clean found just in case
			found = malloc(h.val_len + 1);
			if (!found) {
				free(key_buf);
				free(val_buf);
				return NULL;
			}
			memcpy(found, val_buf, h.val_len + 1);
			found[h.val_len] = '\0'; // Proper null termination
		
			break;
		}
		offset += h.record_len;
		free(key_buf);
		free(val_buf);
	}

	return found;
			
}


