#include "kv.h"
#include "index.h"

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
		if (r < 0 || r != HEADER_LEN) {
			free(found);	
			return NULL; // if not read correctly
		}
		deserialize(header_buf, &h); // deserializes header

		// initialies keybuf with malloc since the size of key_len constantly changes and we dont know this at compile time		
		char *key_buf = malloc(h.key_len + 1);	
		if (!key_buf) {
			free(found);	
			return NULL;
		}

		ssize_t kb = db_read_at(offset + HEADER_LEN, key_buf, h.key_len);
		if (kb < 0 || kb != h.key_len) {
			free(key_buf);
			free(found);	
			return NULL; // if not read correctly
		}
		key_buf[h.key_len] = '\0';
		
		
		if (h.key_len == keylen && strncmp(key, key_buf, keylen) == 0) {
			if (h.record_type == 2) {
				// this record is a tombstone
				free(found);
				found = NULL;

			} else {
                // Found a regular record for this key
                char *val_buf = malloc(h.val_len + 1);	
                if (!val_buf) {
                    free(key_buf); 
                    free(found);	
                    return NULL;
                }	
                
                ssize_t vb = db_read_at(offset + HEADER_LEN + h.key_len, val_buf, h.val_len);
                if (vb < 0 || vb != h.val_len) {
                    free(val_buf);
                    free(key_buf);
                    free(found);	
                    return NULL;
                }
                val_buf[h.val_len] = '\0';
                
                free(found); // clean found just in case
                found = malloc(h.val_len + 1);
                if (!found) {
                    free(key_buf);
                    free(val_buf);
                    return NULL;
                }

                memcpy(found, val_buf, h.val_len);
                found[h.val_len] = '\0';
                free(val_buf);
			}
		}

        free(key_buf);
        offset += h.record_len;
	}

	return found;			
}

int db_delete(const char *key) {
	if (key == NULL) return -1;

	record_header_t record;
	size_t klen = strlen(key);

	record.record_type = 2;
	record.key_len = (uint16_t) strlen(key);
	record.val_len = 0;
	record.record_len = HEADER_LEN + (uint16_t) klen;

	// char buffer we use to hold the data
	char header[HEADER_LEN];
	
	// uses the size of the keys, value, records to seralize the data to a buffer 	
	serialize(&record, header);
	
	// uses the general buffer as way to put the data into the db
	
	if (db_append_raw(header, HEADER_LEN) != 0) return -1;
	if (db_append_raw(key, klen) != 0) return -1;

	return 0;
}

int db_delete_table(const char *key) {
	if (key == NULL || strlen(key) == 0) return -1;
	
	// check if key exists in index
	long exisiting_offset = get(key);
	if (exisiting_offset == -1) return -1; // key doesnt exist

	record_header_t record;
	size_t klen = strlen(key);

	record.record_type = 2; // tombstone
	record.key_len = (uint16_t) strlen(key);
	record.val_len = 0;
	record.record_len = HEADER_LEN + (uint16_t) klen;

	// char buffer we use to hold the data
	char header[HEADER_LEN];
	
	// uses the size of the keys, value, records to seralize the data to a buffer 	
	serialize(&record, header);
	
	// remove from hash table first	
	if (delete(key) != 0) return -1;

	// then write tombstone record to file 
	if (db_append_raw(header, HEADER_LEN) != 0) return -1;
	if (db_append_raw(key, klen) != 0) return -1;

	return 0;

}

// we take the pointer to the key and a pointer to the value
int db_put_table(const char *key, const char *value) {
	if (key == NULL || value == NULL || strlen(key) == 0) return -1;

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
	
	// get the current offset before writing to file
    long current_offset = get_curr_offset();
    if (current_offset == -1) return -1;	
	// insert into hash table first
	if (insert(key, current_offset) != 0) return -1;

	// then write to file
	if (db_append_raw(header, HEADER_LEN) != 0) return -1;
	if (db_append_raw(key, klen) != 0) return -1;
	if (db_append_raw(value, vlen) != 0) return -1;
	
	return 0;
}

char *db_get_table(const char *key) {
	if (!key || strlen(key) == 0) return NULL;

	char header_buf[HEADER_LEN];
	record_header_t h;

	// get offset from hash table
	long offset = get(key);
	if (offset == -1) return NULL; //check if key exists

	ssize_t r = db_read_at(offset, header_buf, HEADER_LEN);

	if (r == 0) return NULL; // checks if we at EOF
	if (r < 0 || r != HEADER_LEN) {
		return NULL; // if not read correctly
	}

	deserialize(header_buf, &h); // deserializes header

	// if the record is a tombstone we can not go through it
	if (h.record_type == 2) {
		return NULL;
	}

	// initialies keybuf with malloc since the size of key_len constantly changes and we dont know this at compile time		
	char *key_buf = malloc(h.key_len + 1);	
	if (!key_buf) {
		return NULL;
	}

	ssize_t kb = db_read_at(offset + HEADER_LEN, key_buf, h.key_len);
	if (kb < 0 || kb != h.key_len) {
		free(key_buf);
		return NULL; // if not read correctly
	}
	key_buf[h.key_len] = '\0';
	
	// verify key matches
	if (strlen(key) != h.key_len || strncmp(key, key_buf, h.key_len) != 0) {
		free(key_buf);
		return NULL; // key mismatch somethign is wrong with the index
	}

	free(key_buf);

	// read the value
	char *val_buf = malloc(h.val_len + 1);
	    if (!val_buf) {
        return NULL;
    }	
    
    ssize_t vb = db_read_at(offset + HEADER_LEN + h.key_len, val_buf, h.val_len);
    if (vb < 0 || vb != h.val_len) {
        free(val_buf);
        return NULL;
    }
    val_buf[h.val_len] = '\0';

    return val_buf; // return the value
}