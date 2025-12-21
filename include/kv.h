#ifndef KV_H
#define KV_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "io.h"

// Database operations (original linear scan)
int db_put(const char *key, const char *value);
char *db_get(const char *key);
int db_delete(const char *key);

// Database operations (with index)
int db_put_table(const char *key, const char *value);
char *db_get_table(const char *key);
int db_delete_table(const char *key);

#endif
