#include <stdio.h>
#include "io.h"

// p_db_file should be binary file 
static FILE *p_db_file = NULL;

int db_open(const char *path) {
	if (!path) return -1;
	if (p_db_file) return 0; //file already open

	// tries to open the file from the path in read/write binary
	p_db_file = fopen(path, "r+b");
	if (!p_db_file) {

		// if it doesnt exist create it	
		p_db_file = fopen(path, "w+b");
		// if it doesnt work return -1	
		if (!p_db_file) return -1;
	}
	return 0;
	
}

void db_close() {
	if (p_db_file) fclose(p_db_file);
	p_db_file = NULL;
}

int db_create(const char *path) {
	if (!path) return -1;
	
	// creates a new file
	FILE *f = fopen(path, "w+b");
	if (!f) return -1;
	
	// if p_db_file is defined we close the file and then we set it equal to f
	if (p_db_file) fclose(p_db_file);	
	p_db_file = f;
	
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