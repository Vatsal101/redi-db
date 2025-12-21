#include <stdio.h>
#include "io.h"
#include "index.h"

// p_db_file should be binary file 
static FILE *p_db_file = NULL;

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


void db_close() {
    if (p_db_file) {
        fclose(p_db_file);
        p_db_file = NULL;
    }
    
    // Clean up hash table when closing database
    cleanup_hash_table();
}

int db_create(const char *path) {
	if (!path) return -1;
	if (p_db_file) {
        fclose(p_db_file);
    }

	p_db_file = fopen(path, "w+b");
    if (!p_db_file) return -1;
    
    // Initialize hash table when creating a new database
    cleanup_hash_table(); // Clean up any existing hash table first
    if (init_hash_table() != 0) {
        fclose(p_db_file);
        p_db_file = NULL;
        return -1;
    }
    
    return 0;	
}

// Uses fwrite to append to binary file of a serialzed general buffer
// Checks if the length of what was written equals to the inputted length
// need to use fwrite to write to binary file
// it will read from inputted data with count elements of size (size) and then write this into the file 
// fwrite(data, size, count, fptr)
// data = name of the array to be written, size = size of each element, count = number of elements to be written, ftpr - file pointer
// returns the number of items written sucessfully
int db_append_raw(const void *buf, size_t len){
	if (!p_db_file || !buf) return -1;
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
	//flush to make sure its written to OS buffers	
	if (fflush(p_db_file) != 0) {
		return -1;	
	}

	return 0;
}

// since the buffer is not a const it means it can be modified that means that buf is going to be an abstract way 
// to store the data we get from our db when we are done storing it.
// This works by first moving the file pointer to the specific location we want which is decided by the offset number
// Then we read from this location up to the length of the data which we know and then write this info to a buffer which we return
// fread(source_addr, size, count, destination_addr)
// RETURNS: how many bytes were read which should be 11 since that is how many we are expecting might change to be
// ready for partial reads 
int db_read_at(long offset, void *buf, size_t len) {
	if (!p_db_file || !buf) return -1;

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
    if (!p_db_file) return -1;
    
    if (fseek(p_db_file, 0, SEEK_SET) != 0) {
        return -1;
    }
    return 0;	
}

int fill_offset_table() {
    if (!p_db_file) return -1;
    
    if (db_rewind() != 0) return -1;

    char header_buf[HEADER_LEN];
    record_header_t h;

    while(1) {
        // Get current file position BEFORE reading
        long current_pos = ftell(p_db_file);
        if (current_pos == -1) break;

        // Try to read header at current position
        size_t bytes_read = fread(header_buf, 1, HEADER_LEN, p_db_file);
        if (bytes_read == 0) break; // EOF
        if (bytes_read != HEADER_LEN) break; // Incomplete read

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
            insert(key_buf, current_pos);
        } else if (h.record_type == 2) {
            // Tombstone - remove from hash table
            delete(key_buf);
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
	if (!path) return -1;
	if (p_db_file) {
		fclose(p_db_file);
	}; //file already open

    p_db_file = fopen(path, "r+b");
    if (!p_db_file) return -1;
    
    // Initialize hash table when opening an existing database
    cleanup_hash_table(); // Clean up any existing hash table first
    if (init_hash_table() != 0) {
        fclose(p_db_file);
        p_db_file = NULL;
        return -1;
    }
    
    // Rebuild the hash table from the existing data
    if (fill_offset_table() != 0) {
        cleanup_hash_table();
        fclose(p_db_file);
        p_db_file = NULL;
        return -1;
    }
    
    return 0;

}

long get_curr_offset() {
    long current_offset = ftell(p_db_file);

    if (current_offset == -1L) {
		return -1; // somethign is wrong with the offset
	} else {
		return current_offset; // this is the current offset of the file pointer is at
	}

}